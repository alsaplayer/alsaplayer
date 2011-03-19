/*  cdda.h (C) 1999-2003 by Andy Lo A Foe <andy@alsaplayer.org>
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

#ifndef _cdda_h 
#define _cdda_h 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>		/* for fd operations		*/
#include <fcntl.h>		/* for fd operations, too	*/
#ifndef __USE_XOPEN
#define __USE_XOPEN /* needed for swab */
#endif
#include <unistd.h>		/* for close()			*/
#undef __USE_XOPEN
#include <errno.h>		/* errno..     		        */

#endif /* _cdda_h */
