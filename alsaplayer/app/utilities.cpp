/*  utilities.cpp
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
 *
 *
 *  $Id$
 *
*/ 

#include <ctype.h>
#include "utilities.h"

// Case insensitive string comparison - this should surely be in the standard
// library, but doesn't seem to be.
int cmp_nocase(const std::string & s, const std::string & s2)
{
	std::string::const_iterator p = s.begin();
	std::string::const_iterator p2 = s2.begin();

	while (p != s.end() && p2 != s2.end()) {
		if(toupper(*p) != toupper(*p2))
			return (toupper(*p) < toupper(*p2)) ?  -1 : 1;
		++p;
		++p2;
	}
	return (s2.size() == s.size()) ? 0 : (s.size() < s2.size()) ? -1 : 1;
}

// Convert an integer to a string
#include <strstream.h>
std::string inttostring(int a)
{
	// Use ostrstream (because ostringstream often doesn't exist)
	char buf[100];  // Very big (though we're also bounds checked)
	ostrstream ost(buf, 100);
	ost << a << std::ends;
	return std::string(buf);
}

/* Threads and usleep does not work, select is very portable */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
void dosleep(unsigned int msec)
{
	struct timeval time;
	time.tv_sec=0;
	time.tv_usec=msec;

	select(0, NULL, NULL, NULL, &time);
}
