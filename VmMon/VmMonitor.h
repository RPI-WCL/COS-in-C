#ifndef _VM_MONITOR_H
#define _VM_MONITOR_H

/* Called from Node Controller */
int VmMonitor_shutdown_theater_req(
    char        *vmmon_addr,
    char        *return_nc_addr,
    char        *theater );

#endif /* #ifndef _VM_MONITOR_H */
