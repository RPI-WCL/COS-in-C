#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "DebugPrint.h"

#include "cos_msg.h"

#define COS_NUM_MAX_PARSED_MSG   10

static
char *msgid_strings[COS_NUM_MESSAGES] = {
    "CREATE_VM_RESP", "NOTIFY_HIGH_CPU_USAGE", 
    "NOTIFY_LOW_CPU_USAGE", "DESTROY_VM_RESP",
    "LAUNCH_TERMINAL_REQ", "TEST" };

CosMessage* cos_alloc_msg(
    CosMessageID msgid )
{
    CosMessage *msg;

    msg = (CosMessage *)malloc( sizeof( CosMessage ) );
    if (msg == NULL) {
        Dbg_printf( COS, FATAL, "cos_alloc_msg, malloc failed\n" );
        return NULL;
    }

    memset( (void*)msg, 0x00, sizeof( CosMessage ) );
    msg->msgid = msgid;
    
    return msg;
}


void cos_set_return_addr_msg(
    CosMessage  *msg, 
    char        *return_addr )
{
    if (msg && return_addr)
        msg->return_addr = return_addr;
}


CosMessageID cos_get_msgid( char *msgid_string )
{
    int i = 0;
    CosMessageID msgid = CosMessageID_UNKNOWN;
    
    for (i = 0; i <  COS_NUM_MESSAGES; i++) {
        if (strcmp( msgid_string, msgid_strings[i] ) == 0) {
            msgid = (CosMessageID)i;
            break;
        }
    }

    return msgid;
}


char* cos_msgid2str(
    CosMessageID msgid )
{
    return msgid_strings[msgid];
}


void cos_set_create_vm_resp_msg(
    CosMessage  *msg,
    char        *vm_name,
    char        *new_theater,
    int         result )
{
    msg->create_vm_resp_msg.vm_name = vm_name;
    msg->create_vm_resp_msg.new_theater = new_theater;
    msg->create_vm_resp_msg.result = result;
}


void cos_set_notify_cpu_usage_msg(
    CosMessage  *msg,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    int i;

    msg->notify_cpu_usage_msg.vm_name = vm_name;
    for (i = 0; i < CPU_USAGE_HISTORY_LEN; i++)
        msg->notify_cpu_usage_msg.cpu_usage_history[i] = cpu_usage_history[i];
}


void cos_set_destroy_vm_resp_msg(
    CosMessage  *msg,
    char        *vm_name,
    int         result )
{
    msg->destroy_vm_resp_msg.vm_name = vm_name;
    msg->destroy_vm_resp_msg.result = result;
}


void cos_set_launch_terminal_req_msg(
    CosMessage  *msg,
    char        *cmd )
{
    msg->launch_term_req_msg.cmd = cmd;
}


void cos_set_test_msg(
    CosMessage  *msg,
    char        *data )
{
    msg->test_msg.data = data;
}


void cos_dealloc_msg(
    CosMessage  *msg )
{
    free( msg );
}


int cos_stringify_msg(
    CosMessage  *msg,
    char        *buf, /* OUT */
    int         *buflen ) /* INOUT */
{
/* Expected data order in a msg */
/* First line  : NUM\n ; the number of data entries */
/* Second line : MSGID,RETURN_ADDR,PARAM1,PARAM2,.... ; data entries */

    int currlen = 0, num_params = 0, i, retval = 0;
    char tempbuf[512];

    if ( (msg->msgid < CosMessageID_CREATE_VM_RESP) || 
         (CosMessageID_UNKNOWN <= msg->msgid) ) {
        Dbg_printf( COS, ERROR, "cos_stringify_msg, invalid msgID=%d\n", msg->msgid );
        return -1;
    }
    
    /* MSGID */
    strcpy( &tempbuf[currlen], cos_msgid2str( msg->msgid ) );
    currlen += strlen( tempbuf );
    num_params++;

    switch( msg->msgid ) {
    case CosMessageID_CREATE_VM_RESP:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->create_vm_resp_msg.vm_name );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->create_vm_resp_msg.new_theater );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%d", msg->create_vm_resp_msg.result );
        currlen = strlen( tempbuf );
        num_params++;
        break;

    case CosMessageID_NOTIFY_HIGH_CPU_USAGE:
    case CosMessageID_NOTIFY_LOW_CPU_USAGE:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->notify_cpu_usage_msg.vm_name );
        currlen = strlen( tempbuf );
        num_params++;

        i = 0;
        while( msg->notify_cpu_usage_msg.cpu_usage_history[i] ) {
            sprintf( &tempbuf[currlen], ",%lf", msg->notify_cpu_usage_msg.cpu_usage_history[i] );
            currlen = strlen( tempbuf );
            num_params++;
            i++;
        }
        break;

    case CosMessageID_DESTROY_VM_RESP:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->destroy_vm_resp_msg.vm_name );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%d", msg->destroy_vm_resp_msg.result );
        currlen = strlen( tempbuf );
        num_params++;
        break;

    case CosMessageID_LAUNCH_TERMINAL_REQ:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->launch_term_req_msg.cmd );
        currlen = strlen( tempbuf );
        num_params++;
        break;

    case CosMessageID_TEST:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->test_msg.data );
        currlen = strlen( tempbuf );
        num_params++;
        break;

    default:
        Dbg_printf( COS, ERROR, "cos_stringify_msg, illegal message(%d)!!!\n", msg->msgid );
        retval = -1;
        break;
    }

    if (retval == 0) {
        sprintf( buf, "%d\n", num_params ); /* assuming buflen <= strlen(%d\n) */
        
        if (*buflen < currlen + strlen(buf) + 1 ) {
            Dbg_printf( COS, ERROR, "not enough buffer allocated in buf\n" );
            retval = -1;
        }
        else {
            strcat( buf, tempbuf );
            *buflen = strlen(buf);
        }
    }

    return retval;
}


CosMessage* cos_parse_msg(
    char        *buf,
    int         buflen )
{
    CosMessage *msg = NULL;
    char *parsed_msg[COS_NUM_MAX_PARSED_MSG];
    int parsed_params_num, msgid, i, theater_num;

    if (ParseMsg_parse( buf, buflen, parsed_msg, &parsed_params_num ) < 0) {
        Dbg_printf( COS, ERROR, "parse failed\n" );
        return NULL;
    }
    
    msgid = cos_get_msgid( parsed_msg[0] );
    
    switch( msgid ) {
    case CosMessageID_CREATE_VM_RESP:
        msg = cos_alloc_msg( CosMessageID_CREATE_VM_RESP );
        msg->return_addr = parsed_msg[1];
        msg->create_vm_resp_msg.vm_name = parsed_msg[2];
        msg->create_vm_resp_msg.new_theater = parsed_msg[3];
        msg->create_vm_resp_msg.result = atoi( parsed_msg[4] );
        break;

    case CosMessageID_NOTIFY_HIGH_CPU_USAGE:
        msg = cos_alloc_msg( CosMessageID_NOTIFY_HIGH_CPU_USAGE );
        msg->return_addr = parsed_msg[1];
        msg->notify_cpu_usage_msg.vm_name = parsed_msg[2];
        for (i = 0; i < parsed_params_num - 3; i++ )
            msg->notify_cpu_usage_msg.cpu_usage_history[i] = atof( parsed_msg[3 + i] );
        break;

    case CosMessageID_NOTIFY_LOW_CPU_USAGE:
        msg = cos_alloc_msg( CosMessageID_NOTIFY_LOW_CPU_USAGE );
        msg->return_addr = parsed_msg[1];
        msg->notify_cpu_usage_msg.vm_name = parsed_msg[2];
        for (i = 0; i < parsed_params_num - 3; i++ )
            msg->notify_cpu_usage_msg.cpu_usage_history[i] = atof( parsed_msg[3 + i] );
        break;

    case CosMessageID_DESTROY_VM_RESP:
        msg = cos_alloc_msg( CosMessageID_DESTROY_VM_RESP );
        msg->return_addr = parsed_msg[1];
        msg->create_vm_resp_msg.vm_name = parsed_msg[2];
        msg->create_vm_resp_msg.result = atoi( parsed_msg[3] );
        break;

    case CosMessageID_LAUNCH_TERMINAL_REQ:
        msg = cos_alloc_msg( CosMessageID_LAUNCH_TERMINAL_REQ );
        msg->return_addr = parsed_msg[1];
        msg->launch_term_req_msg.cmd = parsed_msg[2];
        break;

    case CosMessageID_TEST:
        msg = cos_alloc_msg( CosMessageID_TEST );
        msg->return_addr = parsed_msg[1];
        msg->test_msg.data = parsed_msg[2];
        break;

    default:
        Dbg_printf( COS, ERROR, "invalid msgid=%d\n", msgid );
        break;
    }

    return msg;
}

