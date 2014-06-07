#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "DebugPrint.h"
#include "NodeManager.h"
#include "Socket.h"
#include "node_msg.h"

#define SENDBUF_LEN 128


static
int send_msg( char *dest_addr, char *return_addr, NodeMessage *msg )
{
    int dest_port, buflen = SENDBUF_LEN;
    char *colon, buf[SENDBUF_LEN], dest_host[16];
    int sock;

    colon = strchr( (const char*)dest_addr, ':' );
    if (colon == NULL) {
        Dbg_printf( NODE, ERROR, "send_msg, ':' not found in the dest_addr=%s\n", dest_addr );
        return -1;
    }
    
    memcpy( dest_host, dest_addr, sizeof(char) * (colon - dest_addr) );
    dest_host[colon - dest_addr] = '\0';
    dest_port = atoi( colon + 1 );

    if ((sock = Socket_create()) < 0 ) {
        Dbg_printf( NODE, ERROR, "send_msg, socket creation failed\n" );
        return -1;
    }

    node_set_return_addr( msg, return_addr );

    if (Socket_connect( sock, dest_host, dest_port ) < 0) {
        Dbg_printf( NODE, ERROR, "send_msg, socket connect failed\n" );
        return -1;
    }

    if (node_stringify_msg( msg, buf, &buflen ) < 0 ) {
        Dbg_printf( NODE, ERROR, "send_msg, msg stringify failed\n" );
        return -1;
    }

    Socket_send_data( sock, buf, buflen );

    Socket_close( sock );

    Dbg_printf( NODE, API, "%s message sent to %s\n", node_msgid2str( msg->msgid ), dest_addr );

    return 0;
}


/* Called from COS Manager */
int NodeManager_create_vm_req(
    char        *node_addr,
    char        *return_cos_addr,
    char        *vmcfg_file,   /* this cfg file must be visible on the NODE's file system */
    char        *peer_theaters[MAX_THEATERS] )    /* the last entry in the arrray must be NULL  */
{
    int retval = 0;
    NodeMessage *msg;

    Dbg_printf( NODE, INFO, "node_addr=%s, return_cos_addr=%s, vmcfg_file=%s\n", 
                node_addr, return_cos_addr, vmcfg_file );

    msg = node_alloc_msg( NodeMessageID_CREATE_VM_REQ );
    node_set_create_vm_req_msg( msg, vmcfg_file, peer_theaters );

    if (send_msg( node_addr, return_cos_addr, msg ) < 0) {
        Dbg_printf( NODE, ERROR, "node_send_msg, node_addr=%s, return_cos_addr=%s, msg=%s failed\n", 
                    node_addr, return_cos_addr, node_msgid2str( msg->msgid ) );
        retval = -1;
    }

    node_dealloc_msg( msg );

    return retval;
}

int NodeManager_destroy_vm_req(
    char        *node_addr,
    char        *return_cos_addr,
    char        *vm_name )
{
    int retval = 0;
    NodeMessage *msg;

    Dbg_printf( NODE, INFO, "node_addr=%s, return_cos_addr=%s, vm_name=%s\n", 
                node_addr, return_cos_addr, vm_name );

    msg = node_alloc_msg( NodeMessageID_DESTROY_VM_REQ );
    node_set_destroy_vm_req_msg( msg, vm_name );

    if (send_msg( node_addr, return_cos_addr, msg ) < 0) {
        Dbg_printf( NODE, ERROR, "node_send_msg, node_addr=%s, return_cos_addr=%s, msg=%s failed\n", 
                    node_addr, return_cos_addr, node_msgid2str( msg->msgid ) );
        retval = -1;
    }

    node_dealloc_msg( msg );

    return retval;
}


/* Called from VM Monitor */
int NodeManager_notify_vm_started(
    char        *node_addr,
    char        *vmmon_addr,
    char        *theater )
{
    int retval = 0;
    NodeMessage *msg;

    Dbg_printf( NODE, INFO, "node_addr=%s, vmmon_addr=%s, theater=%s\n",
                node_addr, vmmon_addr, theater );

    msg = node_alloc_msg( NodeMessageID_NOTIFY_VM_STARTED );
    node_set_notify_vm_started_msg( msg, vmmon_addr, theater );

    if (send_msg( node_addr, vmmon_addr, msg ) < 0) {
        Dbg_printf( NODE, ERROR, "node_send_msg, node_addr=%s, msg=%s failed\n", 
                    node_addr, node_msgid2str( msg->msgid ) );
        retval = -1;
    }

    node_dealloc_msg( msg );

    return retval;
}


int NodeManager_notify_high_cpu_usage(
    char        *node_addr,
    char        *vmmon_addr,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    int retval = 0;
    NodeMessage *msg;

    Dbg_printf( NODE, INFO, "vmmon_addr=%s, cpu_usage_history[0]=%lf\n",
                vmmon_addr, cpu_usage_history[0] );

    msg = node_alloc_msg( NodeMessageID_NOTIFY_HIGH_CPU_USAGE );
    node_set_notify_cpu_usage_msg( msg, vmmon_addr, cpu_usage_history );

    if (send_msg( node_addr, vmmon_addr, msg ) < 0) {
        Dbg_printf( NODE, ERROR, "send_msg, node_addr=%s, msg=%s failed\n", 
                    node_addr, node_msgid2str( msg->msgid ) );
        retval = -1;
    }

    node_dealloc_msg( msg );

    return retval;
}


int NodeManager_notify_low_cpu_usage(
    char        *node_addr,
    char        *vmmon_addr,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    int retval = 0;
    NodeMessage *msg;

    Dbg_printf( NODE, INFO, "vmmon_addr=%s, cpu_usage_history[0]=%lf\n",
                vmmon_addr, cpu_usage_history[0] );

    msg = node_alloc_msg( NodeMessageID_NOTIFY_LOW_CPU_USAGE );
    node_set_notify_cpu_usage_msg( msg, vmmon_addr, cpu_usage_history );

    if (send_msg( node_addr, vmmon_addr, msg ) < 0) {
        Dbg_printf( NODE, ERROR, "send_msg, node_addr=%s, msg=%s failed\n", 
                    node_addr, node_msgid2str( msg->msgid ) );
        retval = -1;
    }

    node_dealloc_msg( msg );

    return retval;
}


int NodeManager_shutdown_theater_resp(
    char        *node_addr,
    char        *vmmon_addr,
    int         result )       /* 0: SUCCESS, -1: FAILED */
{
    int retval = 0;
    NodeMessage *msg;

    Dbg_printf( NODE, INFO, "vmmon_addr=%s, result=%d\n", vmmon_addr, result );

    msg = node_alloc_msg( NodeMessageID_SHUTDOWN_THEATER_RESP );
    node_set_shutdown_theater_resp_msg( msg, vmmon_addr, result );

    if (send_msg( node_addr, vmmon_addr, msg ) < 0) {
        Dbg_printf( NODE, ERROR, "send_msg, node_addr=%s, msg=%s failed\n", 
                    node_addr, node_msgid2str( msg->msgid ) );
        retval = -1;
    }

    node_dealloc_msg( msg );

    return retval;
}
