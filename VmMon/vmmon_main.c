#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "CommonDef.h"
#include "Socket.h"
#include "DebugPrint.h"
#include "NodeManager.h"
#include "vmmon_msg.h"

#define DEFAULT_LISTEN_PORT     5002
#define DEFAULT_THEATER_PORT    4040
#define MAX_RECVBUF_LEN         128

#define ACCEPT_TIMEOUT_MSEC     5000
#define SAMPLING_INTERVAL_MSEC  500

/* CPU usage analysis */
#define HIGH_CPU_USAGE_THRESHOLD        0.70
#define LOW_CPU_USAGE_THRESHOLD         0.03
/* 'K OUT OF N' check
   so long as it is between a few seconds and a few tens of seconds. 
   For a measurement interval of 10 s, we suggest k # 3 and n # 5 for
   the ‘‘k out of n” check; this corresponds to requiring the time
   period of about 3 migrations to exceed the resource threshold
   before we initiate a migration. */
#define K_LOW_CPU       5
#define K_HIGH_CPU      2
#define N               CPU_USAGE_HISTORY_LEN   /* 5 */

#define LOAD_INCREASE_DETECTION_THRESHOLD 1.05  /* 5% increase */
#define LOAD_DECREASE_DETECTION_THRESHOLD 0.95  /* 5% decrease */


typedef enum {
    CpuUsage_HIGH = 0,
    CpuUsage_LOW,
    CpuUsage_NORMAL
} CpuUsage;


static int sock = -1;
static int newsock = -1;
static int listen_port = -1;
static int theater_port = -1;
static int vcpu = -1;
static int cpu_check_iteration = 0;
static double cpu_usage_history[CPU_USAGE_HISTORY_LEN];
static int low_cpu_usage_event_enabled = 0;     /* it turns on once a high cpu event is detected */
char nc_addr[24], vmmon_addr[24];


int dbg_level = ALL;

static
void sigcatch( int sig )
{
    Dbg_printf( VMMON, INFO, "Catched Ctrl-C signal, closing the sockets\n" );
    Socket_close( newsock );
    Socket_close( sock );
    exit( 0 );
}


static
int get_netif_addr( 
    char *netif, char addr[16] )
{
    int sock;
    struct ifreq ifr;

    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    ifr.ifr_addr.sa_family = AF_INET;

    strncpy( ifr.ifr_name, netif, IFNAMSIZ-1 );

    ioctl( sock, SIOCGIFADDR, &ifr );

    sprintf( addr, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr) );

    close( sock );

    return 0;
}


static 
double gettimeofday_sec( void )
{
	struct timeval tv;

	gettimeofday( &tv, NULL );

	return (tv.tv_sec + ( double )tv.tv_usec*1e-6);
}


static
int get_cpu_time( int *cpu_time )
{
	char buf[64];
	int usr, nice, sys;
    FILE *fp;

	fp = fopen( "/proc/stat", "r" );
	if ( NULL == fp ) return -1;
	fscanf( fp, "%s %d %d %d", buf, &usr, &nice, &sys );
	fclose( fp );

    *cpu_time = usr + nice + sys;

	return 0;
}


static
double get_cpu_usage( void )
{
    clock_t t1, t2;
    int cpu_time1, cpu_time2;
    struct tms process_times;
    double cpu_usage = 0.0;

    /* cpu usage calculation from /proc/stat */
    t1 = times( &process_times );
    get_cpu_time( &cpu_time1 );
    usleep( SAMPLING_INTERVAL_MSEC * 1000 ); 
    t2 = times( &process_times );
    get_cpu_time( &cpu_time2 );
    cpu_usage = (double)(cpu_time2 - cpu_time1 ) / (t2 - t1);

    return cpu_usage;
}


static
int exec_command( char *param1, char *param2, char *param3, 
                  char *param4, char *param5, char *param6, int child_waittime_msec )
{
    pid_t pid, child_pid;
    int status, retval = 0;
	sigset_t mask, orig_mask;
	struct timespec timeout;

    /* Part of this function is taken from the below web page */
    /* http://www.linuxprogrammingblog.com/code-examples/signal-waiting-sigtimedwait */

    /* Set mask for signals */
	sigemptyset( &mask );
	sigaddset( &mask, SIGCHLD );
	if (sigprocmask( SIG_BLOCK, &mask, &orig_mask ) < 0) {
		perror ( "sigprocmask" );
		return -1;
	}

    /* fork a child process */
    pid = fork ();

    if (pid < 0) {
        Dbg_printf( VMMON, FATAL, "exec_command(), fork failed\n" );
        exit( 1 );
    }

    if (pid == 0) {
        /* CHILD PROCESS */

        Dbg_printf( VMMON, DEBUG, "params={%s,%s,%s,%s,%s,%s}\n", 
                    param1, param2, param3, param4, param5, param6 );

        retval = execlp( param1, param2, param3, param4, param5, param6, NULL );
        if (retval == -1)
            exit( 1 ); /* failure */
        else
            exit( 0 ); /* success */
    }
    else {
        /* PARENT PROCESS: wait for the child to finish with timeout */

        timeout.tv_sec = child_waittime_msec / 1000;
        timeout.tv_nsec = (child_waittime_msec % 1000) * 1000;
        do {
            if (sigtimedwait( &mask, NULL, &timeout ) < 0) {
                if (errno == EINTR) {
                    /* Interrupted by a signal other than SIGCHLD. */
                    continue;
                }
                else if (errno == EAGAIN) {
                    Dbg_printf( VMMON, INFO, "Timeout, killing child\n" );
                    kill (pid, SIGKILL);
                }
                else {
                    perror ("sigtimedwait");
                    return -1;
                }
            }
            break;
        } while ( 1 );
 
        if (waitpid( pid, NULL, 0 ) < 0) {
            perror ("waitpid");
            return -1;
        }

        retval = 0;
    }
        
    return retval;
}


static
int notify_vm_started( char *nc_addr, char *vmmon_addr, char *theater )
{
    Dbg_printf( VMMON, DEBUG, "nc_addr=%s, vmmon_addr=%s, theater=%s\n", nc_addr, vmmon_addr, theater );

    if (NodeManager_notify_vm_started( nc_addr, vmmon_addr, theater ) < 0) {
        Dbg_printf( VMMON, ERROR, "notify_vm_stareted failed\n" );
        return -1;
    }
    
    return 0;
}


static
int shutdown_theater_req( char *theater )
{
    Dbg_printf( VMMON, DEBUG, "theater=%s\n", theater );

#if 0
    if (exec_command( "/usr/bin/java", "java", "-Dnetif=eth0",
                      "-Djava.class.path=/home/imais/Development/vm/mycos/Lib/salsa0.7.2.jar /home/imais/Development/vm/mycos/Lib/IOS0.4.jar .", 
                      "src.testing.reachability.Leave", theater ) < 0) {
        Dbg_printf( VMMON, ERROR, "exec_command failed\n" );
        return -1;
    }
#else
    if (exec_command( "/bin/sh", "sh", "/home/imais/Development/vm/mycos/VmMon/leave", theater, NULL, NULL, 10000 /* msec */) < 0 ) {
        Dbg_printf( VMMON, ERROR, "exec_command failed\n" );
        return -1;
    }
#endif

    /* assuming above exec_command always succeeds */
    if (NodeManager_shutdown_theater_resp( nc_addr, vmmon_addr, 0 /*SUCCESS*/ ) < 0) {
        Dbg_printf( VMMON, ERROR, "NodeManager_shutdown_theater_resp\n" );
        return -1;
    }

    return 0;
}


/* increase & decrease check */
static
CpuUsage analyze_cpu_usage1( double cpu_usage, int vcpu )
{
    int i, analysis = CpuUsage_NORMAL;

    /* fill up history data */
    if (cpu_check_iteration < CPU_USAGE_HISTORY_LEN) {
        cpu_usage_history[cpu_check_iteration] = cpu_usage;
        return CpuUsage_NORMAL;
    }

    if ((((double)vcpu * HIGH_CPU_USAGE_THRESHOLD) < cpu_usage) &&
        (cpu_usage_history[0] * LOAD_INCREASE_DETECTION_THRESHOLD < cpu_usage))
        analysis = CpuUsage_HIGH; /* the load is increasing */
    else
    if ((cpu_usage < ((double)vcpu * LOW_CPU_USAGE_THRESHOLD)) &&
        (cpu_usage < cpu_usage_history[0] * LOAD_DECREASE_DETECTION_THRESHOLD))
        analysis = CpuUsage_LOW; /* the load is decreasing */
    
    /* update cpu_usage_history */
    for (i = 0; i < CPU_USAGE_HISTORY_LEN - 1; i++)
        cpu_usage_history[i] = cpu_usage_history[i + 1];
    cpu_usage_history[CPU_USAGE_HISTORY_LEN - 1] = cpu_usage;

    return analysis;
}


/* K out of N */
static
CpuUsage analyze_cpu_usage2( double cpu_usage, int vcpu )
{
    int i, analysis = CpuUsage_NORMAL;
    int high_threshold_count = 0, low_threshold_count = 0;

    /* fill up history data */
    if (cpu_check_iteration < CPU_USAGE_HISTORY_LEN) {
        cpu_usage_history[cpu_check_iteration] = cpu_usage;
        return CpuUsage_NORMAL;
    }

    if (CPU_USAGE_HISTORY_LEN < N) {
        /* Dbg_printf( VMMON, WARN, "Invalid history_len, history_len(%d) is smaller than N(%d)\n", */
        /*             CPU_USAGE_HISTORY_LEN, N ); */
        return CpuUsage_NORMAL;
    }        

    /* update cpu_usage_history */
    for (i = 0; i < CPU_USAGE_HISTORY_LEN - 1; i++)
        cpu_usage_history[i] = cpu_usage_history[i + 1];
    cpu_usage_history[CPU_USAGE_HISTORY_LEN - 1] = cpu_usage;

    /* K out of N check */
    for (i = 0; i < N; i++) {
        if (HIGH_CPU_USAGE_THRESHOLD <= cpu_usage_history[i]) 
            high_threshold_count++;
        else 
        if (low_cpu_usage_event_enabled && (cpu_usage_history[i] <= LOW_CPU_USAGE_THRESHOLD))
            low_threshold_count++;
    }

    Dbg_printf( VMMON, DEBUG, "CPU_USAGE=%lf, HIGH_COUNT=%d/%d, LOW_COUNT=%d/%d\n",
                cpu_usage, high_threshold_count, K_HIGH_CPU, low_threshold_count, K_LOW_CPU );

    if (K_HIGH_CPU <= high_threshold_count) {
        analysis = CpuUsage_HIGH;
        low_cpu_usage_event_enabled = 1;
    }
    /* CHEAT!!! */
    /* else  */
    /* if (K_LOW_CPU <= low_threshold_count)  */
    /*     analysis = CpuUsage_LOW; */

    return analysis;
}


static
void check_cpu_usage( void )
{
    double cpu_usage = 0.0;            

    cpu_usage = get_cpu_usage();
    /* Dbg_printf( VMMON, DEBUG, "CURRENT CPU USAGE=%lf\n", cpu_usage ); */

    switch (analyze_cpu_usage2( cpu_usage, vcpu )) {
    case CpuUsage_HIGH:
        Dbg_printf( VMMON, DEBUG, "HIGH CPU USAGE(%lf) DETECTED!!!\n", cpu_usage );
        if (NodeManager_notify_high_cpu_usage( nc_addr, vmmon_addr, cpu_usage_history ) < 0) {
            Dbg_printf( VMMON, ERROR, "NodeManager_notify_high_cpu_usage failed\n" );
            break;
        }
        break;

    case CpuUsage_LOW:
        #if 0
        Dbg_printf( VMMON, DEBUG, "LOW cpu usage(%lf) detected, IGNORE it for now!!!\n", cpu_usage );
        #else
        Dbg_printf( VMMON, DEBUG, "LOW CPU USAGE(%lf) DETECTED!!!\n", cpu_usage );
        if (NodeManager_notify_low_cpu_usage( nc_addr, vmmon_addr, cpu_usage_history ) < 0) {
            Dbg_printf( VMMON, ERROR, "NodeManager_notify_low_cpu_usage failed\n" );
            break;
        }
        #endif
        break;

    case CpuUsage_NORMAL:
    default:
        break;
    }
}


int main( int argc, char *argv[] )
{
    int buflen, retval;
    char buf[MAX_RECVBUF_LEN], eth0_addr[16], theater[24];
    VmMessage *msg;

    listen_port = DEFAULT_LISTEN_PORT;
    if (argc == 3) {
        /* nc_addr & vcpu must be given as a parameter */
        strcpy( nc_addr, argv[1] );
        vcpu = atoi( argv[2] );
        listen_port = DEFAULT_LISTEN_PORT;
        theater_port = DEFAULT_THEATER_PORT;
    }
    else if (argc == 4) {
        strcpy( nc_addr, argv[1] );
        vcpu = atoi( argv[2] );
        listen_port = atoi( argv[3] );
        theater_port = DEFAULT_THEATER_PORT;
    }
    else if (argc == 5) {
        strcpy( nc_addr, argv[1] );
        vcpu = atoi( argv[2] );
        listen_port = atoi( argv[3] );
        theater_port = atoi( argv[4] );
    }
    else {
        Dbg_printf( VMMON, ERROR, "Usage: ./VmMonitor [NodeManager IP addr] [#vcpu] [listen port] [theater port]\n" );
        return -1;
    }

    /* setup a handler for Ctrl-C */
    if ( SIG_ERR == signal ( SIGINT, sigcatch ) ) {
        Dbg_printf( VMMON, ERROR, "Failed to set signal handler\n" );
        exit( 1 );
    }

    /* create a socket and start listening */
    if ((sock = Socket_create()) < 0) {
        Dbg_printf( VMMON, ERROR, "Failed to create the socket\n" );
        exit( 1 );
    }
    if (Socket_bind( sock, listen_port ) < 0) {
        Dbg_printf( VMMON, ERROR, "Failed to bind the socket\n" );
        exit( 1 );
    }
    if (Socket_listen( sock ) < 0) {
        Dbg_printf( VMMON, ERROR, "Failed to listen the socket\n" );
        exit( 1 );
    }

    Dbg_printf( VMMON, DEBUG, "VM Monitor has started!!\n" ); 
    get_netif_addr( "eth0", eth0_addr );
    sprintf( theater, "%s:%d", eth0_addr, theater_port );
    sprintf( vmmon_addr, "%s:%d", eth0_addr, listen_port );
    notify_vm_started( nc_addr ,vmmon_addr, theater );

    Socket_set_timeout( sock, ACCEPT_TIMEOUT_MSEC /*ms*/);

    while( 1 ) {
        Dbg_printf( VMMON, DEBUG, "VM MONITOR LISTENING %s\n", vmmon_addr );

        /* accepting data */
        buflen = MAX_RECVBUF_LEN;
        newsock = Socket_accept( sock );
        
        if ((newsock < 0) && (errno == EINTR)) {
            Dbg_printf( VMMON, ERROR, "system interrupt occured\n" );
            continue;
        }

        if (newsock < 0) {
            Dbg_printf( VMMON, DEBUG, "CPU_CHECK_ITERATION=%d\n", cpu_check_iteration );
            check_cpu_usage();
            cpu_check_iteration++;
            continue;
        }

        Socket_set_blocking( newsock );
        Dbg_printf( VMMON, DEBUG, "Request accepted\n" );

        Socket_receive_data( newsock, buf, &buflen ); /* buf is NULL teminated when returned */
        if (buflen == 0)
            continue;

        msg = vmmon_parse_msg( buf, buflen );
        if (msg == NULL) {
            Dbg_printf( VMMON, ERROR, "vmmon_parse_msg failed\n" );
            continue;
        }

        Dbg_printf( VMMON, API, "%s message received from %s\n", vmmon_msgid2str( msg->msgid ), msg->return_addr );

        switch (msg->msgid) {
        case VmMessageID_SHUTDOWN_THEATER_REQ:
            shutdown_theater_req( msg->shutdown_theater_req_msg.theater );
            break;

        default:
            break;
        }

        vmmon_dealloc_msg( msg );
        Socket_close( newsock );
    }
}
