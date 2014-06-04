#ifndef _NODE_MANAGER_H
#define _NODE_MANAGER_H

#include "CommonDef.h"

/* Called from Cluster Controller */
int NodeManager_create_vm_req(
    char        *node_addr,
    char        *return_cos_addr,
    char        *vmcfg_file,   /* this cfg file must be visible on the NC's file system */
    char        *peer_theaters[MAX_THEATERS] );    /* the last entry in the arrray must be NULL  */

int NodeManager_destroy_vm_req(
    char        *node_addr,
    char        *return_cos_addr,
    char        *vm_name );

/* Called from VM Monitor */
int NodeManager_notify_vm_started(
    char        *node_addr,
    char        *vmmon_addr,
    char        *theater );

int NodeManager_notify_high_cpu_usage(
    char        *node_addr,
    char        *vmmon_addr,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

int NodeManager_notify_low_cpu_usage(
    char        *node_addr,
    char        *vmmon_addr,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

int NodeManager_shutdown_theater_resp(
    char        *node_addr,
    char        *vmmon_addr,
    int         result );

#endif /* #ifndef _NODE_MANAGER_H */
