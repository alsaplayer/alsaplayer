/*  external.cpp - external control for alsaplayer
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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
#include "control.h"
#include "packet.h"
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern "C" {

int
ap_connect_session (int session)
{
  int socket_fd;
  struct sockaddr_un saddr;
	struct passwd *pwd;

	pwd = getpwuid(geteuid());

  if ((socket_fd = socket (AF_UNIX, SOCK_STREAM, 0)) != -1) {
    saddr.sun_family = AF_UNIX;
    sprintf (saddr.sun_path, "/tmp/alsaplayer_%s_%d", pwd == NULL ?
					"anonymous" : pwd->pw_name, session);
    if (connect (socket_fd, (struct sockaddr *) &saddr, sizeof (saddr)) != -1) {
      return socket_fd;
    }
	}
	return -1;
}


int
ap_read_packet (int fd, ap_pkt_t * pkt)
{
	if (!pkt)
		return -1;
	pkt->pld_length = 0;	
  if (read (fd, pkt, sizeof (ap_pkt_t)) != sizeof (ap_pkt_t))
		return -1;
  if (pkt->pld_length) {
    pkt->payload = malloc (pkt->pld_length);
    if (read (fd, pkt->payload, pkt->pld_length) != pkt->pld_length) {
			free(pkt->payload);
			return -1;
		}	
  }
	return 1;
}


int
ap_write_packet (int fd, ap_pkt_t pkt)
{
  pkt.version = AP_CONTROL_VERSION;
  if (write (fd, &pkt, sizeof (ap_pkt_t)) != sizeof (ap_pkt_t))
		return -1;
  if (pkt.pld_length) {
    if (write (fd, pkt.payload, pkt.pld_length) != pkt.pld_length)
			return -1;
  }
	return 1;
}


int
ap_do (int session, ap_cmd_t cmd)
{
  ap_pkt_t pkt;
  int fd;

  if ((fd = ap_connect_session (session)) == -1)
    return -1;
  pkt.cmd = cmd;
  pkt.pld_length = 0;
  ap_write_packet (fd, pkt);
  close (fd);
}


int
ap_get_int (int session, ap_cmd_t cmd, int *val)
{
  ap_pkt_t pkt;
  int fd;
  int state = -1;

  if ((fd = ap_connect_session (session)) == -1)
    return -1;
  ap_read_packet (fd, &pkt);
  if (pkt.pld_length == sizeof (int)) {
    state = *(int *) pkt.payload;
  }
  close (fd);
  return state;
}

int
ap_set_int (int session, ap_cmd_t cmd, int val)
{
  ap_pkt_t pkt;
  int fd;
  if ((fd = ap_connect_session (session)) == -1)
    return -1;
  pkt.cmd = cmd;
  pkt.pld_length = sizeof (int);
  pkt.payload = &val;
  ap_write_packet (fd, pkt);
  close (fd);

	return 1;
}


int
ap_set_float (int session, ap_cmd_t cmd, float val)
{
  ap_pkt_t pkt;
  int fd;
  if ((fd = ap_connect_session (session)) == -1)
    return -1;
  pkt.cmd = cmd;
  pkt.pld_length = sizeof (float);
  pkt.payload = &val;
  ap_write_packet (fd, pkt);
  close (fd);

	return 1;
}


int
ap_get_float (int session, ap_cmd_t cmd, float *val)
{
  ap_pkt_t pkt;
  int fd;
  if ((fd = ap_connect_session (session)) == -1)
    return -1;

  pkt.cmd = cmd;
  pkt.pld_length = 0;
  if (ap_write_packet (fd, pkt) == -1)
		return -1;
	if (ap_read_packet (fd, &pkt) == -1)
		return -1;
  if (pkt.pld_length) {
    *val = *(float *) pkt.payload;
    free (pkt.payload);
  }
  close (fd);
  return 1;
}


int ap_set_string(int session, ap_cmd_t cmd, char *str)
{
	ap_pkt_t pkt;
	int fd;

	if ((fd = ap_connect_session (session)) == -1 || !str) 
		return -1;
		
	pkt.cmd = cmd;
	pkt.pld_length = strlen(str);
	pkt.payload = str;

	ap_write_packet(fd, pkt);
	close(fd);
	
	return 1;
}


int 
ap_get_string(int session, ap_cmd_t cmd, char *result)
{
	ap_pkt_t pkt;
	int fd;
	
	if ((fd = ap_connect_session (session)) == -1) 
		return -1;
	
	pkt.cmd = cmd;
	pkt.pld_length = 0;
	ap_write_packet(fd, pkt);
	ap_read_packet(fd, &pkt);
	if (pkt.pld_length && result) {
		memcpy(result, pkt.payload, pkt.pld_length);
		result[pkt.pld_length] = 0; // Null terminate the string
	} else {
		return -1;
	}
	return 1;
}


int ap_session_running(int session)
{
	struct stat statbuf;
	struct passwd *pwd;
	char path[1024];
	float ping;

	pwd = getpwuid(geteuid());
	
	sprintf(path, "/tmp/alsaplayer_%s_%d", pwd == NULL ? "anonymous" :
		pwd->pw_name, session);
	if (stat(path, &statbuf) != 0) 
		return -1;
	if (S_ISSOCK(statbuf.st_mode)) {
		if (ap_get_float(session, AP_GET_FLOAT_PING, &ping) != -1)
			return 1;
	}
	return -1;
}


int ap_find_session(char *session_name)
{
	int i = 0;
	char remote_name[1024];
	
	if (!session_name)
		return -1;
	for (int i=0; i < MAX_AP_SESSIONS; i++) {
		if (ap_session_running(i) == 1) {
			if (ap_get_string(i, AP_GET_STRING_SESSION_NAME, remote_name) != -1) {
				if (strcmp(remote_name, session_name) == 0)
					return i;
			}
		}
	}
	return -1;
}


} /* extern "C" */

