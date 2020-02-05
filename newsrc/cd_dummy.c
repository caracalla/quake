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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "quakedef.h"

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = true;
static qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		playTrack;
static byte		maxTrack;

static int cdfile = -1;
static char cd_dev[64] = "/dev/cdrom";

static void CDAudio_Eject(void) { }

static void CDAudio_CloseDoor(void) { }

static int CDAudio_GetAudioDiskInfo(void) {
	return 0;
}

void CDAudio_Play(byte track, qboolean looping) {}

void CDAudio_Stop(void) {}

void CDAudio_Pause(void) {}

void CDAudio_Resume(void) {}

static void CD_f (void) {}

void CDAudio_Update(void) {}

int CDAudio_Init(void) {
	return 0;
}

void CDAudio_Shutdown(void) {}