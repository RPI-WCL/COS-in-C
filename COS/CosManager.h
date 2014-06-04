#ifndef _COS_MANAGER_H
#define _COS_MANAGER_H

#include "CommonDef.h"

/* Called from Node Manager */
int CosManager_create_vm_resp(
    char        *cc_addr,
    char        *nc_addr,
    char        *vm_name,
    char        *new_theater,   /* assuming one theater per VM */
    int         result );       /* 0: SUCCESS, -1: FAILED */

int CosManager_notify_high_cpu_usage(
    char        *cc_addr,
    char        *nc_addr,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

int CosManager_notify_low_cpu_usage(
    char        *cc_addr,
    char        *nc_addr,
    char        *vm_name,
    double      cpu_usage_history[CPU_USAGE_HISTORY_LEN] );

int CosManager_destroy_vm_resp(
    char        *cc_addr,
    char        *nc_addr,
    char        *vm_name,
    int         result );       /* 0: SUCCESS, -1: FAILED */


#endif /* #ifndef _COS_MANAGER_H */
