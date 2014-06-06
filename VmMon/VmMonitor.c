#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "DebugPrint.h"
#include "Socket.h"
#include "vmmon_msg.h"

#define SENDBUF_LEN 512


static
int send_msg( char *dest_addr, char *return_addr, VmMessage *msg )
{
    int dest_port, buflen = SENDBUF_LEN;
    char *colon, buf[SENDBUF_LEN], dest_host[16], return_host;
    int sock;

    colon = strchr( (const char*)dest_addr, ':' );
    if (colon == NULL) {
        Dbg_printf( VMMON, ERROR, "send_msg, ':' not found in the dest_addr=%s\n", dest_addr );
        return -1;
    }
    
    memcpy( dest_host, dest_addr, sizeof(char) * (colon - dest_addr) );
    dest_host[colon - dest_addr] = '\0';
    dest_port = atoi( colon + 1 );

    if ((sock = Socket_create()) < 0 ) {
        Dbg_printf( VMMON, ERROR, "send_msg, socket creation failed\n" );
        return -1;
    }

    vmmon_set_return_addr_msg( msg, return_addr );

    if (Socket_connect( sock, dest_host, dest_port ) < 0) {
        Dbg_printf( VMMON, ERROR, "send_msg, socket connect failed\n" );
        return -1;
    }

    if (vmmon_stringify_msg( msg, buf, &buflen ) < 0 ) {
        Dbg_printf( VMMON, ERROR, "send_msg, msg stringify failed\n" );
        return -1;
    }

    Socket_send_data( sock, buf, buflen );

    Socket_close( sock );

    Dbg_printf( VMMON, API, "%s message sent to %s\n", vmmon_msgid2str( msg->msgid ), dest_addr );

    return 0;
}


int VmMonitor_shutdown_theater_req(
    char        *vmmon_addr,
    char        *return_nc_addr,
    char        *theater )
{
    int retval = 0;
    VmMessage *msg;

    Dbg_printf( VMMON, INFO, "vmmon_addr=%s, return_nc_addr=%s, theater=%s\n", vmmon_addr, return_nc_addr, theater );

    msg = vmmon_alloc_msg( VmMessageID_SHUTDOWN_THEATER_REQ );
    vmmon_set_shutdown_theater_req_msg( msg, theater );

    if (send_msg( vmmon_addr, return_nc_addr, msg ) < 0) {
        Dbg_printf( VMMON, ERROR, "send_msg, vmmon_addr=%s, msg=%s failed\n", 
                    vmmon_addr, vmmon_msgid2str( msg->msgid ) );
        retval = -1;
    }

    vmmon_dealloc_msg( msg );

    return retval;
}


int VmMonitor_start_vm_req(
    char        *vmmon_addr,
    char        *return_nc_addr)
{
    int retval = 0;
    VmMessage *msg;

    Dbg_printf( VMMON, INFO, "vmmon_addr=%s, return_nc_addr=%s\n", vmmon_addr, return_nc_addr );

    msg = vmmon_alloc_msg( VmMessageID_START_VM_REQ );

    if (send_msg( vmmon_addr, return_nc_addr, msg ) < 0) {
        Dbg_printf( VMMON, ERROR, "send_msg, vmmon_addr=%s, msg=%s failed\n", 
                    vmmon_addr, vmmon_msgid2str( msg->msgid ) );
        retval = -1;
    }

    vmmon_dealloc_msg( msg );

    return retval;
}
