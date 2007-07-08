/*  AlsaSubscriber.h
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsaplayer.org>
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
 *  $Id$
*/ 

#ifndef __AlsaSubscriber_h__
#define __AlsaSubscriber_h__

#include "AlsaNode.h"
#include <pthread.h>


class AlsaSubscriber
{
 private:
	AlsaNode *the_node;
	int the_ID;
	int preferred_pos;
 public:
	AlsaSubscriber();
	~AlsaSubscriber();
	void Subscribe(AlsaNode *node, int pos=POS_BEGIN);
	void Unsubscribe();
	void EnterStream(streamer_type str, void *arg);
	void ExitStream();	
};

#endif
