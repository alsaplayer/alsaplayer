/*
 * drv_alsaplayer.c
 * Copyright (C) 1999 Paul N. Fisher <rao@gnu.org>
 *
 *  This file is part of AlsaPlayer.
 *
 *  AlsaPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AlsaPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mikmod.h"

static BOOL alsaplayer_Init (void)
{
	return VC_Init ();
}

static void alsaplayer_Exit (void)
{
	VC_Exit();
}

static void alsaplayer_Update (void)
{
	/* 
	   No need to use this extra level of indirection;
	   we handle all audio_buffer updating in mikmod_play_frame ()
	 */
}

static BOOL alsaplayer_IsThere (void)
{
	return 1;
}

MDRIVER drv_alsaplayer =
{
	NULL,
	"AlsaPlayer",
	"AlsaPlayer Output Driver v1.0",
	0, 255,
	"alsaplayer",
#if (LIBMIKMOD_VERSION >= 0x030200)
        "",
#endif

	NULL,
	alsaplayer_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	alsaplayer_Init,
	alsaplayer_Exit,
	NULL,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	alsaplayer_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};
