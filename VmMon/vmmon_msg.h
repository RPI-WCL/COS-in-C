#ifndef _VMMON_MSG_H
#define _VMMON_MSG_H 

#include "CommonDef.h"

#define VMMON_NUM_MESSAGES       2

/****** type definitions ******/

typedef enum {
/* Messages from Node Controller */
    VmMessageID_SHUTDOWN_THEATER_REQ = 0,
    VmMessageID_START_VM_REQ,

    VmMessageID_UNKNOWN
} VmMessageID;

typedef struct {
    char        *theater;
} VmShutdownTheaterReqMsg;

typedef struct {
    VmMessageID msgid;
    char        *return_addr; /* host:port */
    union {
        VmShutdownTheaterReqMsg shutdown_theater_req_msg;
    };
} VmMessage;


/****** function declarations ******/

VmMessage* vmmon_alloc_msg(
    VmMessageID msg_id );

void vmmon_set_return_addr_msg(
    VmMessage   *msg, 
    char        *return_addr );

VmMessageID vmmon_get_msgid( char *msgid_string );

char* vmmon_msgid2str( VmMessageID msgid );

void vmmon_set_shutdown_theater_req_msg(
    VmMessage   *msg,
    char        *theater );

void vmmon_dealloc_msg(
    VmMessage   *msg );

int vmmon_stringify_msg(
    VmMessage   *msg,
    char        *buf,   /* OUT */
    int         *buflen );

VmMessage* vmmon_parse_msg(
    char        *buf,
    int         buflen );

#endif /* #ifndef _VMMON_MSG_H */
