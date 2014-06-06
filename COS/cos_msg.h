#ifndef _COS_MESSAGE_H
#define _COS_MESSAGE_H 

#include "CommonDef.h"

#define COS_NUM_MESSAGES        6

typedef enum {
    /* response from Node Controllers */
    CosMessageID_CREATE_VM_RESP = 0,
    CosMessageID_NOTIFY_HIGH_CPU_USAGE,
    CosMessageID_NOTIFY_LOW_CPU_USAGE,
    CosMessageID_DESTROY_VM_RESP,
    CosMessageID_LAUNCH_TERMINAL_REQ,
    CosMessageID_TEST,

    CosMessageID_UNKNOWN
} CosMessageID;

typedef struct {
    char        *vm_name;
    char        *new_theater;
    int         result;
} CosCreateVmRespMsg;

typedef struct {
    char        *vm_name;
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN];
} CosNotifyCpuUsageMsg;

typedef struct {
    char        *vm_name;
    int         result;
} CosDestroyVmRespMsg;

typedef struct {
    char        *cmd;
    int         result;
} CosLaunchTerminalReqMsg;

typedef struct {
    char        *data;
} CosTestMsg;

typedef struct {
    CosMessageID msgid;
    char        *return_addr;
    union {
        CosCreateVmRespMsg       create_vm_resp_msg;
        CosNotifyCpuUsageMsg     notify_cpu_usage_msg;
        CosDestroyVmRespMsg      destroy_vm_resp_msg;
        CosLaunchTerminalReqMsg  launch_term_req_msg;
        CosTestMsg               test_msg;
    };
} CosMessage;


/****** function declarations ******/

CosMessage* cos_alloc_msg(
    CosMessageID msgid );

void cos_set_return_addr_msg(
    CosMessage  *msg, 
    char        *return_addr );

void cos_set_create_vm_resp_msg(
    CosMessage  *msg,
    char        *vm_name,
    char        *new_theater,
    int         result );

void cos_set_notify_cpu_usage_msg(
    CosMessage  *msg,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

void cos_set_destroy_vm_resp_msg(
    CosMessage  *msg,
    char        *vm_name,
    int         result );

void cos_set_launch_terminal_req_msg(
    CosMessage  *msg,
    char        *cmd );

void cos_test_msg(
    CosMessage  *msg,
    char        *data );

void cos_dealloc_msg(
    CosMessage  *msg );

int cos_stringify_msg(
    CosMessage  *msg,
    char        *buf, /* OUT */
    int         *buflen ); /* INOUT */

CosMessage* cos_parse_msg(
    char        *buf,
    int         buflen );

char* cos_msgid2str(
    CosMessageID msgid );

#endif /* #ifndef _COS_MESSAGE_H */
