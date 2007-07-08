/*  error.h -  error functions 
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
 *
 *  This file is part of AlsaPlayer
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
 *  $Id$
 *  
*/ 
#ifndef __alsaplayer_error_h__
#define __alsaplayer_error_h__

#ifdef __cplusplus
extern "C" {
#endif

extern void (*alsaplayer_error)(const char *fmt, ...);
void alsaplayer_set_error_function (void (*func)(const char *, ...));


#ifdef __cplusplus
};
#endif

#endif /* __alsaplayer_error_h__ */
