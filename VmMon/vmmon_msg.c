#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "DebugPrint.h"
#include "vmmon_msg.h"

#define VMMON_NUM_MAX_PARSED_MSG      10

static 
char *msgid_strings[VMMON_NUM_MESSAGES] = {
    "SHUTDOWN_THEATER_REQ" };

VmMessage* vmmon_alloc_msg(
    VmMessageID msgid )
{
    VmMessage *msg;

    msg = (VmMessage *)malloc( sizeof( VmMessage ) );
    if (msg == NULL) {
        Dbg_printf( VMMON, FATAL, "malloc failed\n" );
        return NULL;
    }

    memset( (void*)msg, 0x00, sizeof( VmMessage ) );
    msg->msgid = msgid;
    
    return msg;
}


void vmmon_set_return_addr_msg(
    VmMessage   *msg,
    char        *return_addr )
{
    if (msg && return_addr)
        msg->return_addr = return_addr;
}


VmMessageID vmmon_get_msgid( char *msgid_string )
{
    int i = 0;
    VmMessageID msgid = VmMessageID_UNKNOWN;
    
    for (i = 0; i <  VMMON_NUM_MESSAGES; i++) {
        if (strcmp( msgid_string, msgid_strings[i] ) == 0) {
            msgid = (VmMessageID)i;
            break;
        }
    }

    return msgid;
}


char* vmmon_msgid2str( VmMessageID msgid )
{
    return msgid_strings[msgid];
}


void vmmon_set_shutdown_theater_req_msg(
    VmMessage   *msg,
    char        *theater )
{
    msg->shutdown_theater_req_msg.theater = theater;
}


void vmmon_dealloc_msg(
    VmMessage   *msg )
{
    free( msg );
}

int vmmon_stringify_msg(
    VmMessage   *msg,
    char        *buf,   /* OUT */
    int         *buflen )
{
/* Expected data order in a msg */
/* First line  : NUM\n ; the number of data entries */
/* Second line : MSGID,RETURN_ADDR,PARAM1,PARAM2,.... ; data entries */

    int currlen = 0, num_params = 0, i, retval = 0;
    char tempbuf[128];

    if ( (msg->msgid < VmMessageID_SHUTDOWN_THEATER_REQ) || (VmMessageID_UNKNOWN <= msg->msgid) ) {
        Dbg_printf( VMMON, ERROR, "vmmon_stringify_msg, invalid msgID=%d\n", msg->msgid );
        return -1;
    }
    
    /* MSGID */
    strcpy( &tempbuf[currlen], vmmon_msgid2str( msg->msgid ) );
    currlen += strlen( tempbuf );
    num_params++;

    switch( msg->msgid ) {
    case VmMessageID_SHUTDOWN_THEATER_REQ:
        sprintf( &tempbuf[currlen], ",%s", msg->return_addr );
        currlen = strlen( tempbuf );
        num_params++;

        sprintf( &tempbuf[currlen], ",%s", msg->shutdown_theater_req_msg.theater );
        currlen = strlen( tempbuf );
        num_params++;
        break;
        
    default:
        Dbg_printf( VMMON, ERROR, "vmmon_stringify_msg, illegal message(%d)!!!\n", msg->msgid );
        retval = -1;
        break;
        break;
    }

    if (retval == 0) {
        sprintf( buf, "%d\n", num_params ); /* assuming buflen <= strlen(%d\n) */
        
        if (*buflen < currlen + strlen(buf) + 1 ) {
            Dbg_printf( VMMON, ERROR, "not enough buffer allocated in buf\n" );
            retval = -1;
        }
        else {
            strcat( buf, tempbuf );
            *buflen = strlen(buf);
        }
    }

    return retval;
}


VmMessage* vmmon_parse_msg(
    char        *buf,
    int         buflen )
{
    VmMessage *msg = NULL;
    char *parsed_msg[VMMON_NUM_MAX_PARSED_MSG];
    int parsed_params_num, msgid, i, theater_num;

    if (ParseMsg_parse( buf, buflen, parsed_msg, &parsed_params_num ) < 0) {
        Dbg_printf( VMMON, ERROR, "parse failed\n" );
        return NULL;
    }
    
    msgid = vmmon_get_msgid( parsed_msg[0] );
    
    switch( msgid ) {
    case VmMessageID_SHUTDOWN_THEATER_REQ:
        msg = vmmon_alloc_msg( VmMessageID_SHUTDOWN_THEATER_REQ );
        msg->return_addr = parsed_msg[1];
        msg->shutdown_theater_req_msg.theater = parsed_msg[2];
        break;

    default:
        Dbg_printf( VMMON, ERROR, "invalid msgid=%d\n", msgid );
        break;
    }

    return msg;
}
