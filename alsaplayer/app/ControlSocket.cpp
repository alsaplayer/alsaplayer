#include "ControlSocket.h"
#include "Playlist.h"

static pthread_t socket_thread;
static int socket_thread_running = 0;


void socket_thread_start(Playlist *p)
{
	pthread_create(&socket_thread, NULL, (void * (*)(void *))socket_looper, playlist);
}


void socket_thread_stop()
{
	socket_thread_running = 0;
	pthread_join(socket_thread, NULL);
}

void socket_looper(void *arg)
{
		int socket_fd = 0;

		Playlist *playlist = (Playlist *)arg;
		CorePlayer *player;
		fd_set set;
		struct timeval tv;
		struct sockaddr_un saddr;
		void *data;
		float float_val;
		socklen_t len;
		int fd;
		ap_pkt_t pkt;
		
		socket_thread_running = 1;
	
		if (!playlist) {
			alsaplayer_error("No playlist for control socket\n");
			return;
		}
		unlink("/tmp/alsaplayer_0");
		if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
			saddr.sun_family = AF_UNIX;
			sprintf(saddr.sun_path, "/tmp/alsaplayer_%d", 0);
			if (bind(socket_fd, (struct sockaddr *) &saddr, sizeof (saddr)) != -1) {
				listen(socket_fd, 100);
			} else {
				alsaplayer_error("Error listening on control socket\n");
				return;
			}
		} else {
			alsaplayer_error("Error setting up control socket\n");
			return;
		}	
		while (socket_thread_running) {
			FD_ZERO(&set);
			FD_SET(socket_fd, &set);
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			len = sizeof (saddr);
			data = NULL;

			if ((select(socket_fd + 1, &set, NULL, NULL, &tv) <= 0) ||
					((fd = accept(socket_fd, (struct sockaddr *) &saddr, &len)) == -1))
					continue;
			// So we have a connection
			read(fd, &pkt, sizeof(ap_pkt_t));
			// Read the data blurb 
			if (pkt.pld_length && ((data = malloc(pkt.pld_length)) != NULL)) {
				read(fd, data, pkt.pld_length);
			}	
			switch(pkt.cmd) {
				case AP_DO_PLAY: 
					player = playlist->GetCorePlayer();
					if (player)
						player->Start();
					break;
				case AP_DO_NEXT: 
					playlist->Next(1);
					break;
				case AP_DO_PREV:
					playlist->Prev(1);
					break;
				case AP_DO_STOP:
					player = playlist->GetCorePlayer();
					if (player)
						player->Stop();
					break;
				case AP_DO_PAUSE:
					player = playlist->GetCorePlayer();
					if (player)
						player->SetSpeed(0.0);
					break;
				case AP_SET_FLOAT_SPEED:
					player = playlist->GetCorePlayer();
					if (player)
						player->SetSpeed(*(float *)data);
					break;
				case AP_GET_FLOAT_SPEED:
					player = playlist->GetCorePlayer();
					if (player) {
						float_val = player->GetSpeed();
						pkt.pld_length = sizeof(float);
						write (fd, &pkt, sizeof(ap_pkt_t));
						write(fd, &float_val, pkt.pld_length);
					}
					break;
				case AP_SET_FLOAT_VOLUME:
					player = playlist->GetCorePlayer();
					if (player)
						player->SetVolume(*(float *)data);
					break;
				case AP_GET_FLOAT_VOLUME:
					player = playlist->GetCorePlayer();
					if (player) {
						float_val = player->GetVolume();
						pkt.pld_length = sizeof(float);
						write (fd, &pkt, sizeof(ap_pkt_t));
						write(fd, &float_val, pkt.pld_length);
					}
					break;
				default: alsaplayer_error("CMD = %x\n", pkt.cmd);
					break;
			}
			if (pkt.pld_length)
				free(data);
			close(fd);	
		}
		unlink("/tmp/alsaplayer_0");
}


