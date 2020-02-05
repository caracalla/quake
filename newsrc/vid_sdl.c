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

#define RES_X 640
#define RES_Y 480

cvar_t m_filter = {"m_filter", "0", true};  // mouse smoothing?

qboolean mouse_avail;  // checks if mouse has been inited

int mouse_buttons = 3;  // assume 3 mouse buttons
int mouse_oldbuttonstate;
int mouse_buttonstate;

float mouse_x;
float mouse_y;
float old_mouse_x;
float old_mouse_y;

viddef_t vid;  // global video state
unsigned short d_8to16table[256];

int num_shades = 32;

int d_con_indirect = 0;

int vid_buffersize;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *textureSDL;
// SDL_Surface *screenSurface;
SDL_Event event;

static qboolean oktodraw = false;

static byte current_palette[768];

static long SDL_highhunkmark;
static long SDL_buffersize;

int vid_surfcachesize;
void *vid_surfcache;

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

typedef uint32_t PIXEL24;

const size_t pixel_buffer_size = RES_X * RES_Y * sizeof(PIXEL24);

typedef struct {
	char data[pixel_buffer_size];
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
	r_mask = 0xFF0000; // x_vis->red_mask;
	g_mask = 0xFF00; // x_vis->green_mask;
	b_mask = 0xFF; // x_vis->blue_mask;

	for (r_shift =- 8, x = 1; x < r_mask; x = x << 1) {
		r_shift++;
	}

	for (g_shift =- 8, x = 1; x < g_mask; x = x << 1) {
		g_shift++;
	}

	for (b_shift =- 8, x = 1; x < b_mask; x = x << 1) {
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
	int xi;
	int yi;
	unsigned char *src;
	PIXEL24 *dest;
	register int count;
	register int n;

	if ((x < 0) || (y < 0)) {
		return;
	}

	// x: 0
	// y: 0
	// width: 640
	// height: 480

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
// Tragic death handler
// ========================================================================

void TragicDeath(int signal_num) {
	Sys_Error("This death brought to you by the number %d\n", signal_num);
}

void ResetFrameBuffer(void) {
	int mem;
	int pwidth;

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
	int pnum;  // used for window sizee

	pixel_buffer.bytes_per_line = RES_X * sizeof(PIXEL24);

	vid.width = RES_X; // 320;
	vid.height = RES_Y; // 200;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	srandom(getpid());

	// catch signals so i can turn on auto-repeat
	{
		struct sigaction sa;
		sigaction(SIGINT, 0, &sa);
		sa.sa_handler = TragicDeath;
		sigaction(SIGINT, &sa, 0);
		sigaction(SIGTERM, &sa, 0);
	}

	// if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
	// 	SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
	// }

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
			SDL_PIXELFORMAT_RGBX8888,
			SDL_TEXTUREACCESS_STATIC,
			vid.width,
			vid.height);
	// screenSurface = SDL_GetWindowSurface(window);

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

	vid.rowbytes = pixel_buffer.bytes_per_line; // x_framebuffer[0]->bytes_per_line;
	vid.buffer = pixel_buffer.data; // x_framebuffer[0]->data;
	vid.direct = 0;
	vid.conbuffer = pixel_buffer.data; // x_framebuffer[0]->data;
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

// Called at shutdown

void VID_Shutdown(void) {
	Con_Printf("VID_Shutdown\n");
	SDL_Quit();
}

#if 0
int XLateKey(XKeyEvent *ev) {
	int key;
	char buf[64];
	KeySym keysym;

	key = 0;

	XLookupString(ev, buf, sizeof buf, &keysym, 0);

	switch(keysym) {
		case XK_KP_Page_Up:
		case XK_Page_Up:
			key = K_PGUP;
			break;

		case XK_KP_Page_Down:
		case XK_Page_Down:
			key = K_PGDN;
			break;

		case XK_KP_Home:
		case XK_Home:
			key = K_HOME;
			break;

		case XK_KP_End:
		case XK_End:
			key = K_END;
			break;

		case XK_KP_Left:
		case XK_Left:
			key = K_LEFTARROW;
			break;

		case XK_KP_Right:
		case XK_Right:
			key = K_RIGHTARROW;
			break;

		case XK_KP_Down:
		case XK_Down:
			key = K_DOWNARROW;
			break;

		case XK_KP_Up:
		case XK_Up:
			key = K_UPARROW;
			break;

		case XK_Escape:
			key = K_ESCAPE;
			break;

		case XK_KP_Enter:
		case XK_Return:
			key = K_ENTER;
			break;

		case XK_Tab:
			key = K_TAB;
			break;

		case XK_F1:
			key = K_F1;
			break;

		case XK_F2:
			key = K_F2;
			break;

		case XK_F3:
			key = K_F3;
			break;

		case XK_F4:
			key = K_F4;
			break;

		case XK_F5:
			key = K_F5;
			break;

		case XK_F6:
			key = K_F6;
			break;

		case XK_F7:
			key = K_F7;
			break;

		case XK_F8:
			key = K_F8;
			break;

		case XK_F9:
			key = K_F9;
			break;

		case XK_F10:
			key = K_F10;
			break;

		case XK_F11:
			key = K_F11;
			break;

		case XK_F12:
			key = K_F12;
			break;

		case XK_BackSpace:
			key = K_BACKSPACE;
			break;

		case XK_KP_Delete:
		case XK_Delete:
			key = K_DEL;
			break;

		case XK_Pause:
			key = K_PAUSE;
			break;

		case XK_Shift_L:
		case XK_Shift_R:
			key = K_SHIFT;
			break;

		case XK_Execute:
		case XK_Control_L:
		case XK_Control_R:
			key = K_CTRL;
			break;

		case XK_Alt_L:
		case XK_Meta_L:
		case XK_Alt_R:
		case XK_Meta_R:
			key = K_ALT;
			break;

		case XK_KP_Begin:
			key = K_AUX30;
			break;

		case XK_Insert:
		case XK_KP_Insert:
			key = K_INS;
			break;

		case XK_KP_Multiply:
			key = '*';
			break;
		case XK_KP_Add:
			key = '+';
			break;
		case XK_KP_Subtract:
			key = '-';
			break;
		case XK_KP_Divide:
			key = '/';
			break;

#if 0
		case 0x021:
			key = '1';
			break;/* [!] */

		case 0x040:
			key = '2';
			break;/* [@] */

		case 0x023:
			key = '3';
			break;/* [#] */

		case 0x024:
			key = '4';
			break;/* [$] */

		case 0x025:
			key = '5';
			break;/* [%] */

		case 0x05e:
			key = '6';
			break;/* [^] */

		case 0x026:
			key = '7';
			break;/* [&] */

		case 0x02a:
			key = '8';
			break;/* [*] */

		case 0x028:
			key = '9';;
			break;/* [(] */

		case 0x029:
			key = '0';
			break;/* [)] */

		case 0x05f:
			key = '-';
			break;/* [_] */

		case 0x02b:
			key = '=';
			break;/* [+] */

		case 0x07c:
			key = '\'';
			break;/* [|] */

		case 0x07d:
			key = '[';
			break;/* [}] */

		case 0x07b:
			key = ']';
			break;/* [{] */

		case 0x022:
			key = '\'';
			break;/* ["] */

		case 0x03a:
			key = ';';
			break;/* [:] */

		case 0x03f:
			key = '/';
			break;/* [?] */

		case 0x03e:
			key = '.';
			break;/* [>] */

		case 0x03c:
			key = ',';
			break;/* [<] */

#endif

		default:
			key = *(unsigned char*)buf;

			if (key >= 'A' && key <= 'Z') {
				key = key - 'A' + 'a';
			}

			// fprintf(stdout, "case 0x0%x: key = ___;break;/* [%c] */\n", keysym);
			break;
	}

	return key;
}
#endif

struct {
	int key;
	int down;
} keyq[64];

int keyq_head = 0;
int keyq_tail = 0;

int config_notify = 0;
int config_notify_width;
int config_notify_height;

void GetEvent(void) {
	// XEvent x_event;
	// int b;
	//
	// XNextEvent(x_disp, &x_event);
	//
	// switch(x_event.type) {
	// 	case KeyPress:
	// 		keyq[keyq_head].key = XLateKey(&x_event.xkey);
	// 		keyq[keyq_head].down = true;
	// 		keyq_head = (keyq_head + 1) & 63;
	// 		break;
	//
	// 	case KeyRelease:
	// 		keyq[keyq_head].key = XLateKey(&x_event.xkey);
	// 		keyq[keyq_head].down = false;
	// 		keyq_head = (keyq_head + 1) & 63;
	// 		break;
	//
	// 	case MotionNotify:
	// 		mouse_x = (float) ((int)x_event.xmotion.x - (int)(vid.width / 2));
	// 		mouse_y = (float) ((int)x_event.xmotion.y - (int)(vid.height / 2));
	//
	// 		/* move the mouse to the window center again */
	// 		XSelectInput(
	// 				x_disp,
	// 				x_win,
	// 				StructureNotifyMask|
	// 						KeyPressMask|
	// 						KeyReleaseMask|
	// 						ExposureMask|
	// 						ButtonPressMask|
	// 						ButtonReleaseMask);
	// 		XWarpPointer(
	// 				x_disp,
	// 				None,
	// 				x_win,
	// 				0,
	// 				0,
	// 				0,
	// 				0,
	// 				(vid.width / 2),
	// 				(vid.height / 2));
	// 		XSelectInput(
	// 				x_disp,
	// 				x_win,
	// 				StructureNotifyMask|
	// 						KeyPressMask|
	// 						KeyReleaseMask|
	// 						ExposureMask|
	// 						PointerMotionMask|
	// 						ButtonPressMask|
	// 						ButtonReleaseMask);
	//
	// 		break;
	//
	// 	case ButtonPress:
	// 		b =- 1;
	//
	// 		if (x_event.xbutton.button == 1) {
	// 			b = 0;
	// 		} else if (x_event.xbutton.button == 2) {
	// 			b = 2;
	// 		} else if (x_event.xbutton.button == 3) {
	// 			b = 1;
	// 		}
	//
	// 		if (b >= 0) {
	// 			mouse_buttonstate |= 1 << b;
	// 		}
	//
	// 		break;
	//
	// 	case ButtonRelease:
	// 		b =- 1;
	//
	// 		if (x_event.xbutton.button == 1) {
	// 			b = 0;
	// 		} else if (x_event.xbutton.button == 2) {
	// 			b = 2;
	// 		} else if (x_event.xbutton.button == 3) {
	// 			b = 1;
	// 		}
	//
	// 		if (b >= 0) {
	// 			mouse_buttonstate &= ~(1 << b);
	// 		}
	//
	// 		break;
	//
	// 	case ConfigureNotify:
	// 		// printf("config notify\n");
	// 		config_notify_width = x_event.xconfigure.width;
	// 		config_notify_height = x_event.xconfigure.height;
	// 		config_notify = 1;
	// 		break;
	//
	// 	default:
	// 		break;
	// }
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

		vid.rowbytes = pixel_buffer.bytes_per_line; // x_framebuffer[0]->bytes_per_line;
		vid.buffer = pixel_buffer.data; // x_framebuffer[0]->data;
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

void Sys_SendKeyEvents(void) {
	// get events from x server
	if (oktodraw) {
		while (SDL_PollEvent(&event)) {
			// GetEvent();
			switch(event.type) {
				case SDL_QUIT:
					exit(0);
					break;
			}
		}

		while (keyq_head != keyq_tail) {
			Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down);
			keyq_tail = (keyq_tail + 1) & 63;
		}
	}
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height) {
	// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void D_EndDirectRect(int x, int y, int width, int height) {
	// direct drawing of the "accessing disk" icon isn't supported under Linux
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
