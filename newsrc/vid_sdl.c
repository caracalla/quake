/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_sdl.c -- SDL video and event driver

#define _BSD


#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "quakedef.h"
#include "d_local.h"


cvar_t m_filter = {"m_filter", "0", true};  // mouse smoothing?

qboolean mouse_avail;  // checks if mouse has been inited

int mouse_buttons = 3;  // assume 3 mouse buttons
int mouse_oldbuttonstate;
int mouse_buttonstate;

float mouse_x;
float mouse_y;
float old_mouse_x;
float old_mouse_y;

// for when the display resolution changes
int config_notify = 0;
int config_notify_width;
int config_notify_height;

viddef_t vid;  // global video state
unsigned short d_8to16table[256];

int num_shades = 32;

int d_con_indirect = 0;

int vid_buffersize;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *textureSDL;
SDL_Event event;

static qboolean oktodraw = false;

static long SDL_highhunkmark;
static long SDL_buffersize;

int vid_surfcachesize;
void *vid_surfcache;

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

typedef uint32_t PIXEL24;

#define RES_X 960 // 640
#define RES_Y 720 // 480
#define PIXEL_BUFFER_SIZE RES_X * RES_Y * sizeof(PIXEL24)

typedef struct {
	unsigned char data[PIXEL_BUFFER_SIZE];
	int bytes_per_line;
} pixeldata;

pixeldata pixel_buffer;

static PIXEL24 st2d_8to24table[256];
static int shiftmask_fl = 0;
static long r_shift;
static long g_shift;
static long b_shift;
static unsigned long r_mask;
static unsigned long g_mask;
static unsigned long b_mask;

void shiftmask_init() {
	unsigned int x;
	r_mask = 0xFF0000;
	g_mask = 0xFF00;
	b_mask = 0xFF;

	for (r_shift = -8, x = 1; x < r_mask; x = x << 1) {
		r_shift++;
	}

	for (g_shift = -8, x = 1; x < g_mask; x = x << 1) {
		g_shift++;
	}

	for (b_shift = -8, x = 1; x < b_mask; x = x << 1) {
		b_shift++;
	}

	shiftmask_fl = 1;
}

PIXEL24 xlib_rgb24(int r, int g, int b) {
	PIXEL24 p;

	if (shiftmask_fl == 0) {
		shiftmask_init();
	}

	p = 0;

	if (r_shift > 0) {
		p = (r << (r_shift)) & r_mask;
	} else if (r_shift < 0) {
		p = (r >> (-r_shift)) & r_mask;
	} else {
		p |= (r & r_mask);
	}

	if (g_shift > 0) {
		p |= (g << (g_shift)) & g_mask;
	} else if (g_shift < 0) {
		p |= (g >> (-g_shift)) & g_mask;
	} else {
		p |= (g & g_mask);
	}

	if (b_shift > 0) {
		p |= (b << (b_shift)) & b_mask;
	} else if (b_shift < 0) {
		p |= (b >> (-b_shift)) & b_mask;
	} else {
		p |= (b & b_mask);
	}

	return p;
}

void st3_fixup(int x, int y, int width, int height) {
	int yi;
	unsigned char *src;
	PIXEL24 *dest;
	register int count;
	register int n;

	if ((x < 0) || (y < 0)) {
		return;
	}

	for (yi = y; yi < (y + height); yi++) {
		src = &pixel_buffer.data[yi * pixel_buffer.bytes_per_line];

		// Duff's Device
		count = width;
		n = (count + 7) / 8;
		dest = ((PIXEL24 *)src) + x + width - 1;
		src += x + width - 1;

		switch (count % 8) {
		case 0:	do {	*dest-- = st2d_8to24table[*src--];
		case 7:			*dest-- = st2d_8to24table[*src--];
		case 6:			*dest-- = st2d_8to24table[*src--];
		case 5:			*dest-- = st2d_8to24table[*src--];
		case 4:			*dest-- = st2d_8to24table[*src--];
		case 3:			*dest-- = st2d_8to24table[*src--];
		case 2:			*dest-- = st2d_8to24table[*src--];
		case 1:			*dest-- = st2d_8to24table[*src--];
				} while (--n > 0);
		}
	}
}

// ========================================================================
// SDL Video Logic
// ========================================================================

void ResetFrameBuffer(void) {
	if (d_pzbuffer) {
		D_FlushCaches();
		Hunk_FreeToHighMark(SDL_highhunkmark);
		d_pzbuffer = NULL;
	}

	SDL_highhunkmark = Hunk_HighMark();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	SDL_buffersize = vid.width * vid.height * sizeof (*d_pzbuffer);

	vid_surfcachesize = D_SurfaceCacheForRes(vid.width, vid.height);

	SDL_buffersize += vid_surfcachesize;

	d_pzbuffer = Hunk_HighAllocName(SDL_buffersize, "video");

	if (d_pzbuffer == NULL) {
		Sys_Error("Not enough memory for video mode\n");
	}

	vid_surfcache =
			(byte *)d_pzbuffer + vid.width * vid.height * sizeof (*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);
}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again
void VID_Init(unsigned char *palette) {
	pixel_buffer.bytes_per_line = RES_X * sizeof(PIXEL24);

	vid.width = RES_X;
	vid.height = RES_Y;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	srandom(getpid());

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		Sys_Quit();
	}

	window = SDL_CreateWindow(
			"quake",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			vid.width,
			vid.height,
			SDL_WINDOW_SHOWN);
	renderer = SDL_CreateRenderer(window, -1, 0);
	textureSDL = SDL_CreateTexture(
			renderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			vid.width,
			vid.height);

	// capture mouse
	SDL_SetRelativeMouseMode(SDL_TRUE);

	// wait for first exposure event
	{
		do {
			SDL_PollEvent(&event);

			if (
					event.type == SDL_WINDOWEVENT &&
					event.window.event == SDL_WINDOWEVENT_SHOWN) {
				oktodraw = true;
			}
		} while (!oktodraw);
	}

	ResetFrameBuffer();

	vid.rowbytes = pixel_buffer.bytes_per_line;
	vid.buffer = (pixel_t *)pixel_buffer.data;
	vid.direct = 0;
	vid.conbuffer = (pixel_t *)pixel_buffer.data;
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
}

void VID_ShiftPalette(unsigned char *p) {
	VID_SetPalette(p);
}

void VID_SetPalette(unsigned char *palette) {
	int i;

	for (i = 0; i < 256; i++) {
		st2d_8to24table[i]= xlib_rgb24(
				palette[i * 3],
				palette[i * 3 + 1],
				palette[i * 3 + 2]);
	}
}

// flushes the given rectangles from the view buffer to the screen
void VID_Update(vrect_t *rects) {
	// if the window changes dimension, skip this frame
	if (config_notify) {
		fprintf(stderr, "config notify\n");
		config_notify = 0;
		vid.width = config_notify_width & ~7;
		vid.height = config_notify_height;

		ResetFrameBuffer();

		vid.rowbytes = pixel_buffer.bytes_per_line;
		vid.buffer = (pixel_t *)pixel_buffer.data;
		vid.conbuffer = vid.buffer;
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
		vid.conrowbytes = vid.rowbytes;
		vid.recalc_refdef = 1;  // force a surface cache flush
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	// always force full update
	extern int scr_fullupdate;
	scr_fullupdate = 0;

	while (rects) {
		st3_fixup(
				rects->x,
				rects->y,
				rects->width,
				rects->height);

		SDL_UpdateTexture(
				textureSDL,
				NULL,
				pixel_buffer.data,
				RES_X * sizeof(PIXEL24));

		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, textureSDL, NULL, NULL);
		SDL_RenderPresent(renderer);
		rects = rects->pnext;
	}
}

void VID_Shutdown(void) {
	Con_Printf("VID_Shutdown\n");
	SDL_Quit();
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height) {
	// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void D_EndDirectRect(int x, int y, int width, int height) {
	// direct drawing of the "accessing disk" icon isn't supported under Linux
}

// ========================================================================
// SDL Keyboard and Mouse Logic
// ========================================================================

struct {
	int key;
	int down;
} keyq[64];

int keyq_head = 0;
int keyq_tail = 0;

int ProcessKey(SDL_Keysym keysym);

void ProcessEvent(void) {
	int button;

	switch(event.type) {
		case SDL_QUIT: {
			Con_Printf("hey thanks for playing have a good day!!!\n");
			Sys_Quit();
			break;
		}

		case SDL_KEYDOWN:
			keyq[keyq_head].key = ProcessKey(event.key.keysym);
			keyq[keyq_head].down = true;
			keyq_head = (keyq_head + 1) & 63;
			break;

		case SDL_KEYUP:
			keyq[keyq_head].key = ProcessKey(event.key.keysym);
			keyq[keyq_head].down = false;
			keyq_head = (keyq_head + 1) & 63;
			break;

		case SDL_MOUSEMOTION:
			mouse_x = (float)event.motion.xrel;
			mouse_y = (float)event.motion.yrel;
			break;

		case SDL_MOUSEBUTTONDOWN:
			button = -1;

			if (event.button.button == 1) {
				button = 0;
			} else if (event.button.button == 2) {
				button = 2;
			} else if (event.button.button == 3) {
				button = 1;
			}

			if (button >= 0) {
				mouse_buttonstate |= 1 << button;
			}

			break;

		case SDL_MOUSEBUTTONUP:
			button = -1;

			if (event.button.button == 1) {
				button = 0;
			} else if (event.button.button == 2) {
				button = 2;
			} else if (event.button.button == 3) {
				button = 1;
			}

			if (button >= 0) {
				mouse_buttonstate &= ~(1 << button);
			}

			break;

		// case ConfigureNotify:
		// 	// printf("config notify\n");
		// 	config_notify_width = x_event.xconfigure.width;
		// 	config_notify_height = x_event.xconfigure.height;
		// 	config_notify = 1;
		// 	break;

		default:
			break;
	}
}

void Sys_SendKeyEvents(void) {
	// get events from x server
	if (oktodraw) {
		while (SDL_PollEvent(&event)) {
			ProcessEvent();
		}

		while (keyq_head != keyq_tail) {
			Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down);
			keyq_tail = (keyq_tail + 1) & 63;
		}
	}
}

void IN_Init(void) {
	Cvar_RegisterVariable(&m_filter);

	if (COM_CheckParm ("-nomouse")) {
		return;
	}

	mouse_x = mouse_y = 0.0;
	mouse_avail = 1;
}

void IN_Shutdown(void) {
	mouse_avail = 0;
}

void IN_Commands(void) {
	int i;

	if (!mouse_avail) {
		return;
	}

	for (i = 0; i < mouse_buttons; i++) {
		if ((mouse_buttonstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i))) {
			Key_Event(K_MOUSE1 + i, true);
		}

		if (!(mouse_buttonstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) {
			Key_Event(K_MOUSE1 + i, false);
		}
	}

	mouse_oldbuttonstate = mouse_buttonstate;
}

void IN_Move(usercmd_t *cmd) {
	if (!mouse_avail) {
		return;
	}

	if (m_filter.value) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

	if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1))) {
		cmd->sidemove += m_side.value * mouse_x;
	} else {
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	}

	if (in_mlook.state & 1) {
		V_StopPitchDrift();
	}

	if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;

		if (cl.viewangles[PITCH] > 80) {
			cl.viewangles[PITCH] = 80;
		}

		if (cl.viewangles[PITCH] < -70) {
			cl.viewangles[PITCH] = -70;
		}
	} else {
		if ((in_strafe.state & 1) && noclip_anglehack) {
			cmd->upmove -= m_forward.value * mouse_y;
		} else {
			cmd->forwardmove -= m_forward.value * mouse_y;
		}
	}

	mouse_x = mouse_y = 0.0;
}

// junk so we don't have to write 26 thousand case statements
#define CONCAT_H(x,y,z) x##y##z
#define SINGLEQUOTE '
#define CONCAT(x,y,z) CONCAT_H(x,y,z)
#define CHARIFY(x) CONCAT(SINGLEQUOTE , x , SINGLEQUOTE )

#define key_scancode(letter) \
		case SDL_SCANCODE_##letter: \
			key = CHARIFY(letter) - 'A' + 'a'; \
			break

int ProcessKey(SDL_Keysym keysym) {
	int key = 0;

	switch(keysym.scancode) {
		case SDL_SCANCODE_LEFT:
			key = K_LEFTARROW;
			break;

		case SDL_SCANCODE_RIGHT:
			key = K_RIGHTARROW;
			break;

		case SDL_SCANCODE_UP:
			key = K_UPARROW;
			break;

		case SDL_SCANCODE_DOWN:
			key = K_DOWNARROW;
			break;

		case SDL_SCANCODE_ESCAPE:
			key = K_ESCAPE;
			break;

		case SDL_SCANCODE_KP_ENTER:
		case SDL_SCANCODE_RETURN:
		case SDL_SCANCODE_RETURN2:
			key = K_ENTER;
			break;

		case SDL_SCANCODE_SPACE:
			key = K_SPACE;
			break;

		case SDL_SCANCODE_BACKSPACE:
			key = K_BACKSPACE;
			break;

		case SDL_SCANCODE_GRAVE:
			key = '`';
			break;

		case SDL_SCANCODE_EQUALS:
			key = '+';  // FIXME: shitty hack to get + to work
			break;

		case SDL_SCANCODE_MINUS:
			key = '-';
			break;

		case SDL_SCANCODE_BACKSLASH:
			key = '\\';
			break;

		key_scancode(0);
		key_scancode(1);
		key_scancode(2);
		key_scancode(3);
		key_scancode(4);
		key_scancode(5);
		key_scancode(6);
		key_scancode(7);
		key_scancode(8);
		key_scancode(9);
		key_scancode(A);
		key_scancode(B);
		key_scancode(C);
		key_scancode(D);
		key_scancode(E);
		key_scancode(F);
		key_scancode(G);
		key_scancode(H);
		key_scancode(I);
		key_scancode(J);
		key_scancode(K);
		key_scancode(L);
		key_scancode(M);
		key_scancode(N);
		key_scancode(O);
		key_scancode(P);
		key_scancode(Q);
		key_scancode(R);
		key_scancode(S);
		key_scancode(T);
		key_scancode(U);
		key_scancode(V);
		key_scancode(W);
		key_scancode(X);
		key_scancode(Y);
		key_scancode(Z);


		default:
			break;
	}

	return key;
}
