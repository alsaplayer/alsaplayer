#ifndef _cdda_h 
#define _cdda_h 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>		/* for fd operations		*/
#include <fcntl.h>		/* for fd operations, too	*/
#define __USE_XOPEN /* needed for swab */
#include <unistd.h>		/* for close()			*/
#undef __USE_XOPEN
#include <errno.h>		/* errno..     		        */

#endif /* _cdda_h */
