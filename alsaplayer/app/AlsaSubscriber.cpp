/*  AlsaSubscriber.cpp - Subscriber class to interface with AlsaNode
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
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
 * $Id$
 *
*/ 

#include "AlsaSubscriber.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sched.h>


AlsaSubscriber::AlsaSubscriber()
{
	the_node = NULL;
	the_ID = -1;
	preferred_pos = POS_BEGIN;
}

AlsaSubscriber::~AlsaSubscriber()
{
	if (the_node && the_node->IsInStream(the_ID))
		ExitStream();
	Unsubscribe();
}


void AlsaSubscriber::Subscribe(AlsaNode *node, int pos)
{
	the_node = node; 
	preferred_pos = pos;
}


void AlsaSubscriber::Unsubscribe()
{
	if (the_node && the_node->IsInStream(the_ID)) { 
		the_node->RemoveStreamer(the_ID);

	}	
	the_node = NULL;
	the_ID = -1;
}


void AlsaSubscriber::EnterStream(streamer_type str, void *arg)
{
	if (the_node) {
		the_ID = the_node->AddStreamer(str, arg, preferred_pos);
		the_node->StartStreaming();
	}
}


void AlsaSubscriber::ExitStream()
{
	if (the_node && the_node->IsInStream(the_ID)) {
		if (!the_node->RemoveStreamer(the_ID)) {
			printf("ERROR! Failed to remove streamer\n");
		} else {
			the_ID = -1;
		}	
	}			
}

