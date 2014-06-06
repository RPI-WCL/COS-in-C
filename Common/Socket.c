#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include "DebugPrint.h"

#define MAX_RECVBUF_LEN 512

int Socket_set_blocking( int sock )
{
    int val;
    val = fcntl( sock, F_GETFL,  0);
    val = val & ~( val & O_NONBLOCK );
    fcntl( sock, F_SETFL, val );
}


int Socket_create( void )
{
    int sock;

    sock = socket( PF_INET, SOCK_STREAM, 0 );
    if (sock < 0) {
        Dbg_printf( COMMON, ERROR, "create_socket(), socket creation failed\n" );
        return -1;
    }

    return sock;
}

int Socket_bind( int sock, int port )
{
    int yes = 1;
	struct sockaddr_in server;

	server.sin_family = PF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( port );

    /* Enable using the port that is in TIME_WAIT state */
    setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof( yes ) );

	if (bind( sock, (struct sockaddr *)&server, sizeof( struct sockaddr_in ) ) < 0 )
	{
		Dbg_printf( COMMON, ERROR, "bind_socket(), bind failed, %s\n", strerror( errno ) );
		return -1;
	}

    return 0;
}


int Socket_listen( int sock )
{
   /* 5 is number of backlogged waiting clients */
    return listen( sock, 5 );
}


int Socket_accept( int sock )
{
	struct sockaddr_in client;
    int fromlen = sizeof( client );

    return accept( sock, (struct sockaddr *)&client, &fromlen );
}


int Socket_connect( int sock, char *host, int port )
{
	struct sockaddr_in addr;
	struct hostent *hp;
	
	addr.sin_family = PF_INET;
	hp = gethostbyname( host );
	bcopy( (char *)hp->h_addr, (char *)&addr.sin_addr, hp->h_length );
	addr.sin_port = htons( port );

	if (connect( sock, (struct sockaddr *)&addr, sizeof( addr) ) < 0) {
		Dbg_printf( COMMON, ERROR, "connect_socket(), connect failed\n" );
		return -1;
	}

    return 0;
}


void Socket_close( int sock )
{
    close( sock );
}


int Socket_send_data( int sock, char *buf, int buflen )
{
    int written_len = 0, sendbuf_len = 0;
    char sendbuf[1024];

    /* sendbuf = (char *)malloc( 64 /\*for the content length *\/ + buflen ); */

    sprintf( sendbuf, "%d\n", buflen );
    strcat( sendbuf, buf );
    sendbuf_len = strlen( sendbuf );

    while ( written_len < sendbuf_len ) {
		written_len += write( sock, &sendbuf[written_len], sendbuf_len - written_len );
		Dbg_printf( COMMON, DEBUG, "Sent data (%d/%d bytes)\n", written_len, sendbuf_len );
	}

	return 0;
}


static
int parse_contlen( char *buf, int buflen, int *currpos, int *contlen )
{
	char *cr;

	buf[buflen] = '\0';

	if ((cr = strchr( &buf[*currpos], '\n' )) == NULL) {
		/* CR not found */
		*contlen = -1;
		return -1;
	}

	/* CR found */
	*cr = '\0';
	*contlen = atoi( buf );
	*currpos = strlen( buf ) + 1; /* should point one byte after CR */
	
	return 0;

}


int Socket_receive_data( int sock, char *buf, int *buflen )
{
	int found_contlen = 0, read_body = 0, readlen = 0, currpos = 0, contlen = 0, tmplen;
    char recvbuf[MAX_RECVBUF_LEN];

	while (!found_contlen || (readlen < currpos + contlen)) {
        tmplen = read( sock, &recvbuf[readlen], MAX_RECVBUF_LEN - readlen );

        if (tmplen < 0) {
            Dbg_printf( COMMON, ERROR, "error occured, %d:%s\n", errno, strerror(errno) );
            return -1;
            /* continue; */
        }
        else
        if (tmplen == 0) {
            Dbg_printf( COMMON, DEBUG, "no more data to read\n" );
            break;
        }

		Dbg_printf( COMMON, DEBUG, "Received data (%d bytes)\n", tmplen );
		readlen += tmplen;

		if (!found_contlen && !parse_contlen( recvbuf, readlen, &currpos, &contlen )) 
			found_contlen = 1;
    }

    if ((*buflen + 1) < contlen) {
        Dbg_printf( COMMON, ERROR, "receive_data(), not enough buffer for copy\n" );
        return -1;
    }

    memcpy( buf, &recvbuf[currpos], contlen );
    buf[contlen] = '\0';
    *buflen = contlen;

	return 0;    
}


int Socket_set_timeout( int sock, int timeout_ms )
{
    int retval = 0;
    struct timeval timeout;

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    return setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout) );
}
