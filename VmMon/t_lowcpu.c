#include "CommonDef.h"
#include "DebugPrint.h"
#include "NodeManager.h"

#define NODE_IPADDR      "cirrus.cs.rpi.edu:5001"  /* cirrus */


int dbg_level = ALL;

int main( int argc, char *argv[] ) 
{
    char *node_addr = NODE_IPADDR, *vmmon_addr;
    double cpu_usage_history[CPU_USAGE_HISTORY_LEN] = {0.0};
    
    Dbg_printf( TEST, DEBUG, "Low CPU usage notificatoin test!!\n" );

    if (argc == 2) {
        vmmon_addr = argv[1];
    }
    else {
        Dbg_printf( TEST, ERROR, "Usage: ./t_lowcpu [VmMonitor address]\n" );
        return -1;
    }

    NodeManager_notify_low_cpu_usage( node_addr, vmmon_addr, cpu_usage_history );

    return 0;
}
