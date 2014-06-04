#ifndef _SOCKET_H
#define _SOCKET_H

/* common */
int Socket_set_blocking( int sock );
int Socket_create( void );
void Socket_close( int sock );
int Socket_send_data( int sock, char *buf, int buflen );
int Socket_receive_data( int sock, char *buf, int *buflen );
int Socket_set_timeout( int sock, int timeout_ms );

/* for servers */
int Socket_bind( int sock, int port );
int Socket_listen( int sock );
int Socket_accept( int sock );

/* for clients */
int Socket_connect( int sock, char *host, int port );

#endif /* #ifndef _SOCKET_H */
