#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "DebugPrint.h"

#include "node_msg.h"

#define NODE_NUM_MAX_PARSED_MSG      10

static 
char *msgid_strings[NODE_NUM_MESSAGES] = {
    "CREATE_VM_REQ", "DESTROY_VM_REQ", "NOTIFY_VM_STARTED",
    "NOTIFY_HIGH_CPU_USAGE", "NOTIFY_LOW_CPU_USAGE", "SHUTDOWN_THEATER_RESP" };

NodeMessage* node_alloc_msg(
    NodeMessageID msgid )
{
    NodeMessage *msg;

    msg = (NodeMessage *)malloc( sizeof( NodeMessage ) );
    if (msg == NULL) {
        Dbg_printf( NODE, FATAL, "node_alloc_msg, malloc failed\n" );
        return NULL;
    }

    memset( (void*)msg, 0x00, sizeof( NodeMessage ) );
    msg->msgid = msgid;
    
    return msg;
}


void node_set_return_addr(
    NodeMessage   *msg,
    char        *return_addr )
{
    if (msg && return_addr)
        msg->return_addr = return_addr;
}


NodeMessageID node_get_msgid( char *msgid_string )
{
    int i = 0;
    NodeMessageID msgid = NodeMessageID_UNKNOWN;
    
    for (i = 0; i <  NODE_NUM_MESSAGES; i++) {
        if (strcmp( msgid_string, msgid_strings[i] ) == 0) {
            msgid = (NodeMessageID)i;
            break;
        }
    }

    return msgid;
}


char* node_msgid2str(
    NodeMessageID msgid )
{
    return msgid_strings[msgid];
}


void node_set_create_vm_req_msg(
    NodeMessage   *msg,
    char        *vmcfg_file,
    char        *peer_theaters[MAX_THEATERS] )
{
    int i;

    msg->create_vm_req_msg.vmcfg_file = vmcfg_file;
    for (i = 0; i < MAX_THEATERS; i++)
        msg->create_vm_req_msg.peer_theaters[i] = peer_theaters[i];    
}


void node_set_destroy_vm_req_msg(
    NodeMessage   *msg,
    char        *vm_name )
{
    msg->destroy_vm_req_msg.vm_name = vm_name;
}


void node_set_notify_vm_started_msg(
    NodeMessage   *msg,
    char        *vmmon_addr,
    char        *theater )
{
    msg->notify_vm_started_msg.vmmon_addr = vmmon_addr;
    msg->notify_vm_started_msg.theater = theater;
}


void node_set_notify_cpu_usage_msg(
    NodeMessage   *msg,
    char        *vmmon_addr,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    int i;

    msg->notify_cpu_usage_msg.vmmon_addr = vmmon_addr;
    for (i = 0; i < CPU_USAGE_HISTORY_LEN; i++)
        msg->notify_cpu_usage_msg.cpu_usage_history[i] = cpu_usage_history[i];
}


void node_set_shutdown_theater_resp_msg(
    NodeMessage   *msg,
    char        *vmmon_addr,
    int         result )
{
    msg->shutdown_theater_resp_msg.vmmon_addr = vmmon_addr;
    msg->shutdown_theater_resp_msg.result = result;
}


void node_dealloc_msg(
    NodeMessage   *msg )
{
    free( msg );
}


int node_stringify_msg(
    NodeMessage   *msg,           /* IN */
    char        *buf,           /* INOUT */
    int         *buflen )       /* INOUT */
{
/* Expected data order in a msg */
/* First line  : NUM\n ; the number of data entries */
/* Second line : MSGID,RETURN_ADDR,PARAM1,PARAM2,.... ; data entries */

    int currlen = 0, num_params = 0, i, retval = 0;
    char tempbuf[512];

    if ( (msg->msgid < NodeMessageID_CREATE_VM_REQ) || (NodeMessageID_UNKNOWN <= msg->msgid) ) {
        Dbg_printf( NODE, ERROR, "node_stringify_msg, invalid msgID=%d\n", msg->msgid );
        return -1;
    }
    
    /* MSGID */
    strcpy( &tempbuf[currlen], node_msgid2str( msg->msgid ) );
    currlen += strlen( tempbuf );
    num_params++;

    switch( msg->msgid ) {
    case NodeMessageID_CREATE_VM_REQ:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->create_vm_req_msg.vmcfg_file );
        currlen = strlen( tempbuf );
        num_params++;

        i = 0;
        while( msg->create_vm_req_msg.peer_theaters[i] ) {
            sprintf( &tempbuf[currlen], ",%s", msg->create_vm_req_msg.peer_theaters[i] );
            currlen = strlen( tempbuf );
            num_params++;
            i++;
        }
        break;

    case NodeMessageID_DESTROY_VM_REQ:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->destroy_vm_req_msg.vm_name );
        num_params++;        
        break;

    case NodeMessageID_NOTIFY_VM_STARTED:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->notify_vm_started_msg.vmmon_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->notify_vm_started_msg.theater );
        currlen = strlen( tempbuf );
        num_params++;
        break;

    case NodeMessageID_NOTIFY_HIGH_CPU_USAGE:
    case NodeMessageID_NOTIFY_LOW_CPU_USAGE:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->notify_cpu_usage_msg.vmmon_addr );
        currlen = strlen( tempbuf );
        num_params++;

        for (i = 0; i < CPU_USAGE_HISTORY_LEN; i++) {
            sprintf( &tempbuf[currlen], ",%lf", msg->notify_cpu_usage_msg.cpu_usage_history[i] );
            currlen = strlen( tempbuf );
            num_params++;
        }
        break;

    case NodeMessageID_SHUTDOWN_THEATER_RESP:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->shutdown_theater_resp_msg.vmmon_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%d", msg->shutdown_theater_resp_msg.result );
        currlen = strlen( tempbuf );
        num_params++;
        break;

    default:
        Dbg_printf( NODE, ERROR, "node_stringify_msg, illegal message(%d)!!!\n", msg->msgid );
        retval = -1;
        break;
    }

    if (retval == 0) {
        sprintf( buf, "%d\n", num_params ); /* assuming buflen <= strlen(%d\n) */
        
        if (*buflen < currlen + strlen(buf) + 1 ) {
            Dbg_printf( NODE, ERROR, "not enough buffer allocated in buf\n" );
            retval = -1;
        }
        else {
            strcat( buf, tempbuf );
            *buflen = strlen(buf);
        }
    }

    return retval;
}


NodeMessage* node_parse_msg(
    char        *buf,
    int         buflen )
{
    NodeMessage *msg = NULL;
    char *parsed_msg[NODE_NUM_MAX_PARSED_MSG];
    int parsed_params_num, msgid, i, theater_num;

    if (ParseMsg_parse( buf, buflen, parsed_msg, &parsed_params_num ) < 0) {
        Dbg_printf( NODE, ERROR, "parse failed\n" );
        return NULL;
    }
    
    msgid = node_get_msgid( parsed_msg[0] );
    
    switch( msgid ) {
    case NodeMessageID_CREATE_VM_REQ:
        msg = node_alloc_msg( NodeMessageID_CREATE_VM_REQ );
        msg->return_addr = parsed_msg[1];
        msg->create_vm_req_msg.vmcfg_file = parsed_msg[2];
        for (i = 0; i < parsed_params_num - 3; i++ )
            msg->create_vm_req_msg.peer_theaters[i] = parsed_msg[3 + i];
        break;

    case NodeMessageID_DESTROY_VM_REQ:
        msg = node_alloc_msg( NodeMessageID_DESTROY_VM_REQ );
        msg->return_addr = parsed_msg[1];
        msg->destroy_vm_req_msg.vm_name = parsed_msg[2];
        break;

    case NodeMessageID_NOTIFY_VM_STARTED:
        msg = node_alloc_msg( NodeMessageID_NOTIFY_VM_STARTED );
        msg->return_addr = parsed_msg[1];
        msg->notify_vm_started_msg.vmmon_addr = parsed_msg[2];
        msg->notify_vm_started_msg.theater = parsed_msg[3];
        break;

    case NodeMessageID_NOTIFY_HIGH_CPU_USAGE:
        msg = node_alloc_msg( NodeMessageID_NOTIFY_HIGH_CPU_USAGE );
        msg->return_addr = parsed_msg[1];
        msg->notify_cpu_usage_msg.vmmon_addr = parsed_msg[2];
        for (i = 0; i < parsed_params_num - 3; i++ )
            msg->notify_cpu_usage_msg.cpu_usage_history[i] = atof( parsed_msg[3 + i] );
        break;

    case NodeMessageID_NOTIFY_LOW_CPU_USAGE:
        msg = node_alloc_msg( NodeMessageID_NOTIFY_LOW_CPU_USAGE );
        msg->return_addr = parsed_msg[1];
        msg->notify_cpu_usage_msg.vmmon_addr = parsed_msg[2];
        for (i = 0; i < parsed_params_num - 3; i++ )
            msg->notify_cpu_usage_msg.cpu_usage_history[i] = atof( parsed_msg[3 + i] );
        break;

    case NodeMessageID_SHUTDOWN_THEATER_RESP:
        msg = node_alloc_msg( NodeMessageID_SHUTDOWN_THEATER_RESP );
        msg->return_addr = parsed_msg[1];
        msg->shutdown_theater_resp_msg.vmmon_addr = parsed_msg[2];
        msg->shutdown_theater_resp_msg.result  = atoi( parsed_msg[3] );
        break;

    default:
        Dbg_printf( NODE, ERROR, "invalid msgid=%d\n", msgid );
        break;
    }

    return msg;
}

