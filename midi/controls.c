/* 

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   controls.c
   
   */

#include "gtim.h"
#include "controls.h"

int last_rc_command = 0;
int last_rc_arg = 0;

int check_for_rc(void) {
  int rc = last_rc_command;
  int32 val;

return 0;
  if (!rc)
    {
	rc = ctl->read(&val);
	last_rc_command = rc;
	last_rc_arg = val;
    }

  switch (rc)
    {
      case RC_QUIT: /* [] */
      case RC_LOAD_FILE:	  
      case RC_NEXT: /* >>| */
      case RC_REALLY_PREVIOUS: /* |<< */
      case RC_PATCHCHANGE:
      case RC_CHANGE_VOICES:
      case RC_STOP:
	return rc;
      default:
	return 0;
    }
}

/* Minimal control mode */

extern ControlMode dumb_control_mode;
#ifndef DEFAULT_CONTROL_MODE
# define DEFAULT_CONTROL_MODE &dumb_control_mode
#endif


ControlMode *ctl_list[]={
  &dumb_control_mode,
  0
};

ControlMode *ctl=DEFAULT_CONTROL_MODE;
