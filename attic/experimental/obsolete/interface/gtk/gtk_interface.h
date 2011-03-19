/*  gtk_interface.h
 *  Copyright (C) 1999 Richard Boulton <richard@tartarus.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _gtk_interface_h_
#define _gtk_interface_h_

#include "CorePlayer.h"
#include "Playlist.h"

#include <gdk/gdkkeysyms.h>

extern gint windows_x_offset;
extern gint windows_y_offset;
extern int global_update;

void init_main_window(Playlist *);


gboolean key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data);

enum gtk_keymap {
	STOP_KEY = GDK_v,
	PLAY_KEY = GDK_x,
	PAUSE_KEY = GDK_c,
	NEXT_KEY = GDK_b,
	PREV_KEY = GDK_z,
	FWD_KEY = GDK_g,
	BACK_KEY = GDK_a,
	FWD_PLAY_KEY = GDK_f,
	REV_PLAY_KEY = GDK_s,
	SPEED_UP_KEY = GDK_t,
	SPEED_DOWN_KEY = GDK_q,
	SPEED_COMMA_UP_KEY = GDK_h,
	SPEED_COMMA_DOWN_KEY = GDK_i,
	VOL_UP_KEY = GDK_r,
	VOL_DOWN_KEY = GDK_w,
	LOOP_KEY = GDK_l
};

#endif /* _gtk_interface_h_ */
