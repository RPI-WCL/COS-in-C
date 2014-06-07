#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "DebugPrint.h"
#include "CosManager.h"
#include "Socket.h"
#include "cos_msg.h"

#define SENDBUF_LEN 512

static char cos_addr[16] = {0};

static 
char* get_netif_addr( 
    char *netif )
{
    int sock;
    struct ifreq ifr;

    if (cos_addr[0] == 0) {
        sock = socket( AF_INET, SOCK_DGRAM, 0 );
        ifr.ifr_addr.sa_family = AF_INET;

        strncpy( ifr.ifr_name, netif, IFNAMSIZ-1 );

        ioctl( sock, SIOCGIFADDR, &ifr );

        sprintf( cos_addr, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr) );

        close( sock );
    }

    return cos_addr;
}


static
int send_msg( char *dest_addr, char *return_addr, CosMessage *msg )
{
    int dest_port, buflen = SENDBUF_LEN;
    char *colon, buf[SENDBUF_LEN], dest_host[16], *return_host;
    int sock;

    colon = strchr( (const char*)dest_addr, ':' );
    if (colon == NULL) {
        Dbg_printf( COS, ERROR, "send_msg, ':' not found in the dest_addr=%s\n", dest_addr );
        return -1;
    }
    
    memcpy( dest_host, dest_addr, sizeof(char) * (colon - dest_addr) );
    dest_host[colon - dest_addr] = '\0';
    dest_port = atoi( colon + 1 );

    if ((sock = Socket_create()) < 0 ) {
        Dbg_printf( COS, ERROR, "send_msg, socket creation failed\n" );
        return -1;
    }

    cos_set_return_addr_msg( msg, return_addr );

    if (Socket_connect( sock, dest_host, dest_port ) < 0) {
        Dbg_printf( COS, ERROR, "send_msg, socket connect failed\n" );
        return -1;
    }

    if (cos_stringify_msg( msg, buf, &buflen ) < 0 ) {
        Dbg_printf( COS, ERROR, "send_msg, msg stringify failed\n" );
        return -1;
    }

    Socket_send_data( sock, buf, buflen );

    Socket_close( sock );

    Dbg_printf( COS, API, "%s message sent to %s\n", cos_msgid2str( msg->msgid ), dest_addr );

    return 0;
}


/* Called from Node Manager */
int CosManager_create_vm_resp(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    char        *new_theater,   /* assuming one theater per VM */
    int         result )        /* 0: SUCCESS, -1: FAILED */
{
    int retval = 0;
    CosMessage *msg;

    Dbg_printf( COS, INFO, "cos_addr=%s, vm_name=%s, new_theater=%s, result=%d\n", 
                cos_addr, vm_name, new_theater, result );

    msg = cos_alloc_msg( CosMessageID_CREATE_VM_RESP );
    cos_set_create_vm_resp_msg( msg, vm_name, new_theater, result );

    if (send_msg( cos_addr, nc_addr, msg ) < 0) {
        Dbg_printf( COS, ERROR, "cos_send_msg, cos_addr=%s, msg=%s failed\n", 
                    cos_addr, cos_msgid2str( msg->msgid ) );
        retval = -1;
    }

    cos_dealloc_msg( msg );

    return retval;
}


int CosManager_notify_high_cpu_usage(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    int retval = 0;
    CosMessage *msg;

    Dbg_printf( COS, INFO, "cos_addr=%s, vm_name=%s\n", 
                cos_addr, vm_name );

    msg = cos_alloc_msg( CosMessageID_NOTIFY_HIGH_CPU_USAGE );
    cos_set_notify_cpu_usage_msg( msg, vm_name, cpu_usage_history );

    if (send_msg( cos_addr, nc_addr, msg ) < 0) {
        Dbg_printf( COS, ERROR, "cos_send_msg, cos_addr=%s, msg=%s failed\n", 
                    cos_addr, cos_msgid2str( msg->msgid ) );
        retval = -1;
    }

    cos_dealloc_msg( msg );

    return retval;
}


int CosManager_notify_low_cpu_usage(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    int retval = 0;
    CosMessage *msg;

    Dbg_printf( COS, INFO, "cos_addr=%s, vm_name=%s\n",
                cos_addr, vm_name );

    msg = cos_alloc_msg( CosMessageID_NOTIFY_LOW_CPU_USAGE );
    cos_set_notify_cpu_usage_msg( msg, vm_name, cpu_usage_history );

    if (send_msg( cos_addr, nc_addr, msg ) < 0) {
        Dbg_printf( COS, ERROR, "cos_send_msg, cos_addr=%s, msg=%s failed\n", 
                    cos_addr, cos_msgid2str( msg->msgid ) );
        retval = -1;
    }

    cos_dealloc_msg( msg );

    return retval;
}


int CosManager_destroy_vm_resp(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    int         result )        /* 0: SUCCESS, -1: FAILED */
{
    int retval = 0;
    CosMessage *msg;

    Dbg_printf( COS, INFO, "cos_addr=%s, vm_name=%s, result=%d\n", 
                cos_addr, vm_name, result );

    msg = cos_alloc_msg( CosMessageID_DESTROY_VM_RESP );
    cos_set_destroy_vm_resp_msg( msg, vm_name, result );

    if (send_msg( cos_addr, nc_addr, msg ) < 0) {
        Dbg_printf( COS, ERROR, "cos_send_msg, cos_addr=%s, msg=%s failed\n", 
                    cos_addr, cos_msgid2str( msg->msgid ) );
        retval = -1;
    }

    cos_dealloc_msg( msg );

    return retval;

}


int CosManager_launch_terminal_req(
    char        *cos_addr,
    char        *sender,        /* node or vm */
    char        *cmd )
{
    int retval = 0;
    CosMessage *msg;

    Dbg_printf( COS, INFO, "cos_addr=%s, sender=%s, cmd=%s\n", 
                cos_addr, sender, cmd );

    msg = cos_alloc_msg( CosMessageID_LAUNCH_TERMINAL_REQ );
    cos_set_launch_terminal_req_msg( msg, cmd );

    if (send_msg( cos_addr, sender, msg ) < 0) {
        Dbg_printf( COS, ERROR, "cos_send_msg, cos_addr=%s, msg=%s failed\n", 
                    cos_addr, cos_msgid2str( msg->msgid ) );
        retval = -1;
    }

    cos_dealloc_msg( msg );

    return retval;
}


int CosManager_test(
    char        *cos_addr,
    char        *data )
{
    int retval = 0;
    CosMessage *msg;

    Dbg_printf( COS, INFO, "cos_addr=%s, data=%s\n",
                cos_addr, data );

    msg = cos_alloc_msg( CosMessageID_TEST );
    cos_set_test_msg( msg, data );

    if (send_msg( cos_addr, NULL, msg ) < 0) {
        Dbg_printf( COS, ERROR, "cos_send_msg, cos_addr=%s, msg=%s failed\n", 
                    cos_addr, cos_msgid2str( msg->msgid ) );
        retval = -1;
    }

    cos_dealloc_msg( msg );

    return retval;
}
