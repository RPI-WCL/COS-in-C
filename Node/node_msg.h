#ifndef _NODE_MSG_H
#define _NODE_MSG_H 

#include "CommonDef.h"

#define NODE_NUM_MESSAGES        6

/****** type definitions ******/

typedef enum {
/* Messages from Cluster Controller */
    NodeMessageID_CREATE_VM_REQ = 0,    /* create & start */
    NodeMessageID_DESTROY_VM_REQ,       /* shutdown theater & destroy VM */

/* Messages from VM Monitor */
    NodeMessageID_NOTIFY_VM_STARTED,
    NodeMessageID_NOTIFY_HIGH_CPU_USAGE,
    NodeMessageID_NOTIFY_LOW_CPU_USAGE,
    NodeMessageID_SHUTDOWN_THEATER_RESP,

    NodeMessageID_UNKNOWN,
} NodeMessageID;


typedef struct {
    char        *vmcfg_file;
    char        *peer_theaters[MAX_THEATERS];
} NodeCreateVmReqMsg;

typedef struct {
    char        *vm_name;
} NodeDestroyVmReqMsg;

typedef struct {
    char        *vmmon_addr;
    char        *theater;
} NodeNotifyVmStartedMsg;

typedef struct {
    char        *vmmon_addr;
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN];
} NodeNotifyCpuUsageMsg;

typedef struct {
    char        *vmmon_addr;
    int         result;
} NodeShutdownTheaterRespMsg;

typedef struct {
    NodeMessageID msgid;
    char        *return_addr; /* host:port */
    union {
        NodeCreateVmReqMsg        create_vm_req_msg;
        NodeDestroyVmReqMsg       destroy_vm_req_msg;
        NodeNotifyVmStartedMsg    notify_vm_started_msg;
        NodeNotifyCpuUsageMsg     notify_cpu_usage_msg;
        NodeShutdownTheaterRespMsg shutdown_theater_resp_msg;
    };
} NodeMessage;


/****** function declarations ******/

NodeMessage* node_alloc_msg(
    NodeMessageID msgid );

void node_set_return_addr_msg(
    NodeMessage *msg, 
    char        *return_addr );

void node_set_create_vm_req_msg(
    NodeMessage *msg,
    char        *vmcfg_file,
    char        *peer_theaters[MAX_THEATERS] );

void node_set_destroy_vm_req_msg(
    NodeMessage *msg,
    char        *vm_name );

void node_notify_vm_started_msg(
    NodeMessage *msg,
    char        *vmmon_addr,
    char        *theater );

void node_notify_cpu_usage_msg(
    NodeMessage *msg,
    char        *vmmon_addr,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

void node_set_shutdown_theater_resp_msg(
    NodeMessage *msg,
    char        *vmmon_addr,
    int         result );

void node_dealloc_msg(
    NodeMessage *msg );

int node_stringify_msg(
    NodeMessage *msg,
    char        *buf, /* OUT */
    int         *buflen ); /* INOUT */

NodeMessage* node_parse_msg(
    char        *buf,
    int         buflen );

char* node_msgid2str(
    NodeMessageID msgid );

#endif /* #ifndef _NODE_MSG_H */
