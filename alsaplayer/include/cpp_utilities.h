/*  cpp_utilities.h
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

#ifndef __cpp_utilities_h__
#define __cpp_utilities_h__

#include <string>

// Case insensitive string comparison - this should surely be in the standard
// library, but doesn't seem to be.
int cmp_nocase(const std::string &, const std::string &);

extern "C" {
// Convert an integer to a string
std::string inttostring(int);
}

#endif
