#ifndef _COS_MANAGER_H
#define _COS_MANAGER_H

#include "CommonDef.h"

/* Called from Node Manager */
int CosManager_create_vm_resp(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    char        *new_theater,   /* assuming one theater per VM */
    int         result );       /* 0: SUCCESS, -1: FAILED */

int CosManager_notify_high_cpu_usage(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

int CosManager_notify_low_cpu_usage(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

int CosManager_destroy_vm_resp(
    char        *cos_addr,
    char        *nc_addr,
    char        *vm_name,
    int         result );       /* 0: SUCCESS, -1: FAILED */

/* Called from Node Manager or VmMonitor */
int CosManager_launch_terminal_req(
    char        *cos_addr,
    char        *sender,        /* node or vm */
    char        *cmd );

/* Debug purpose */
int CosManager_test(
    char        *cos_addr,
    char        *data );

#endif /* #ifndef _COS_MANAGER_H */
