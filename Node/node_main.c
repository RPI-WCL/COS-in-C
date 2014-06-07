#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/times.h>

#include "Socket.h"
#include "DebugPrint.h"
#include "node_msg.h"

#define DEFAULT_LISTEN_PORT     5001
#define MAX_RECVBUF_LEN         512
#define VM_ARRAY_LEN            1       /* use only 1 for now */

#define VMMON_TERMINAL_COLOR    "Blue"
#define VMMON_EXEC_PATH         "Git/COS-in-C/VmMon/VmMonitor"
#define VMMON_VCPU              1
#define VMMON_LISTEN_PORT       5002



typedef enum {
    VmState_NULL = 0,
    VmState_IN_CREATION,
    VmState_IN_CONNECTING_PEERS,
    VmState_ACTIVE,
    VmState_IN_SHUTTING_DOWN
} VmState;

typedef struct {
    int id;
    VmState vm_state;
    /* char vm_addr[16];           /\* IP address of the VM (host) *\/ */
    char vmmon_addr[24];        /* IP address of the VMMonitor (host:port) */
    char vm_name[16];

    char theater[24];
    char *peer_theaters[MAX_THEATERS];
    char peer_theaters_memory[MAX_THEATERS * THEATER_ADDR_LEN];
} VM;

static int sock = -1;
static int newsock = -1;
static int curr_vm_id = 1;
static int listen_port = -1;
static VM vm_array[VM_ARRAY_LEN];
static int num_vm = -1;
static char cos_addr[24], node_addr[24];

int dbg_level = ALL;

static
void sigcatch( int sig )
{
    Dbg_printf( NODE, INFO, "Catched Ctrl-C signal, closing the sockets\n" );
    Socket_close( newsock );
    Socket_close( sock );
    exit( 0 );
}


static 
void get_netif_addr( 
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
}


static 
double gettimeofday_msec( void )
{
	struct timeval tv;

	gettimeofday( &tv, NULL );

	return (tv.tv_sec * 1000 + ( double )tv.tv_usec * 1e-3);
}


static
void init_vm_array( VM vm_array[VM_ARRAY_LEN] )
{
    int i, j;

    /* vm_state will be VmState_NULL */
    memset( (void*)vm_array, 0x00, sizeof( vm_array ) );

    for (i = 0; i < VM_ARRAY_LEN; i++) {
        vm_array[i].id = i;
        for (j = 0; j < MAX_THEATERS; j++)
            vm_array[i].peer_theaters[j] = NULL;
    }
    
    num_vm = i;
}


static
int exec_xm_command( char *param1, char *param2, char *param3, char *param4, char *param5 )
{
    pid_t pid, child_pid;
    int status, retval = 0;

    /* fork a child process */
    pid = fork ();

    if (pid < 0) {
        Dbg_printf( NODE, ERROR, "exec_cm_command(), fork failed\n" );
        exit( 1 );
    }

    if (pid == 0) {
        Dbg_printf( NODE, INFO, "params={%s,%s,%s,%s,%s}\n", param1, param2, param3, param4, param5 );

        /* CHILD PROCESS: request xm vcpu-set execution to Dom0 */
        retval = execlp( "/usr/sbin/xm", "xm", param1, param2, param3, param4, param5, NULL );
        if (retval == -1)
            exit( 1 ); /* failure */
        else
            exit( 0 ); /* success */
    }
    else {
        /* PARENT PROCESS: wait for the child to finish */
        child_pid = wait( &status );

        /* if(WIFEXITED(status)){ */
        /*   child_rc=WEXITSTATUS(status); */
        /*   printf("I'm the parent.\n"); */
        /*   printf("status=%d\n", status ); */
        /*   printf("My child's PID was: %d\n",child_pid); */
        /*   printf("My child's return code was: %d\n",child_rc); */
        /* } else { */
        /*   printf("My child didn't exit normally\n"); */
        /* } */

        /* printf( "PARENT PROCESS, status=%d, WIFEXITED=%d, WEXITSTATUS=%d\n", */
        /*         status, WIFEXITED( status ), WEXITSTATUS( status ) ); */
        /* if (status && WIFEXITED( status ) && !WEXITSTATUS( status )) */
        /*     retval = 0; */
        /* else */
        /*     retval = -1; */

        /* can't get status from the child process.. */
        retval = 0;
    }
        
    return retval;
}

static
VM* find_vm_by_addr( char *vmmon_addr )
{
    int i;
    VM *vm = NULL;
    
    for (i = 0; i < VM_ARRAY_LEN; i++) {
        if (strcmp( vm_array[i].vmmon_addr, vmmon_addr ) == 0) {
            vm = &vm_array[i];
            break;
        }
    }
            
    return vm;
}

static
VM* find_vm_by_state( VmState state )
{
    int i;
    VM *vm = NULL;
    
    for (i = 0; i < VM_ARRAY_LEN; i++) {
        if (vm_array[i].vm_state == state) {
            vm = &vm_array[i];
            break;
        }
    }
            
    return vm;
}


static
VM* find_vm_by_name( char *vm_name )
{
    int i;
    VM *vm = NULL;
    
    for (i = 0; i < VM_ARRAY_LEN; i++) {
        if (strcmp( vm_array[i].vm_name, vm_name ) == 0) {
            vm = &vm_array[i];
            break;
        }
    }
            
    return vm;
}


static
int create_vm_req( 
    char        *vmcfg_file,
    char        *peer_theaters[MAX_THEATERS] )    
{
    VM *vm;
    char *slash, *dot, *c, *vm_name;
    int retval = 0;
    int i;

    Dbg_printf( NODE, INFO, "entered\n" );

    vm = find_vm_by_state( VmState_NULL );

    /* expecting "XXX/YYY.ZZZ" format in vmcfg_file */
    slash = strrchr( (const char*)vmcfg_file, '/' );
    dot = strrchr( (const char*)vmcfg_file, '.' );
    if (dot <= slash) {
        Dbg_printf( NODE, ERROR, "invalid format of cfgfile(%s)\n", vmcfg_file );
        retval = -1;
        goto EXIT;
    }

    /* extract vm_name from vmcfg_file */
    c = slash + 1;
    vm_name = vm->vm_name;
    while (c < dot) {
        *vm_name++ = *c++;
    }
    *vm_name = '\0';
    Dbg_printf( NODE, DEBUG, "extracted vm_name=%s from cfgfile=%s\n", vm->vm_name, vmcfg_file );

    /* execute 'xm create ***.cfg' */
    exec_xm_command( "create", vmcfg_file, NULL, NULL, NULL );

    /* save peers_theaters for later use */
    i = 0;
    memset( (void*)vm->peer_theaters_memory, 0x00, sizeof( vm->peer_theaters_memory ) );
    while (peer_theaters[i]) {
        strcpy( &vm->peer_theaters_memory[i * THEATER_ADDR_LEN], peer_theaters[i] );
        vm->peer_theaters[i] = &vm->peer_theaters_memory[i * THEATER_ADDR_LEN];
        i++;
    }

EXIT:
    if (retval < 0) {
        vm->vm_state = VmState_NULL;
        if (CosManager_create_vm_resp( cos_addr, node_addr, vm->vm_name, vm->theater, -1 /*FAIL*/ ) < 0)
            Dbg_printf( NODE, ERROR, "CosManager_create_vm_resp failed\n" );
    }
    else {
        vm->vm_state = VmState_IN_CREATION;
    }

    return retval;
}


static
int start_vm_terminal( char *dest_ipaddr )
{
    VM *vm;
    char cmd[256], exec_path[64];

    vm = find_vm_by_state( VmState_NULL );
    strcpy( vm->vm_name, node_addr );   /* this time, node_addr == vm_addr */

    strcpy( exec_path, getenv("HOME") );
    strcat( exec_path, "/" );
    strcat( exec_path, VMMON_EXEC_PATH );
    sprintf( cmd, "gnome-terminal --title=\"VmMonitor@%s:%d\" --window-with-profile=\"%s\" --command=\"ssh user@%s %s %s %s %d %d\"", 
             dest_ipaddr, VMMON_LISTEN_PORT, VMMON_TERMINAL_COLOR, dest_ipaddr,
             exec_path, cos_addr, node_addr, VMMON_VCPU, VMMON_LISTEN_PORT );

    if (CosManager_launch_terminal_req( cos_addr, node_addr, cmd ) < 0) {
        Dbg_printf( NODE, ERROR, "CosManager_launch_terminal_req failed\n" );
    }
    else {
        vm->vm_state = VmState_IN_CREATION;
    }
}


static
int destroy_vm_req( 
    char        *vm_name )
{
    VM *vm;
    int retval = 0;

    Dbg_printf( NODE, INFO, "vm_name=%s\n", vm_name );


    if ((vm = find_vm_by_name( vm_name )) == NULL) {
        Dbg_printf( NODE, ERROR, "valid vm management entry not found, vm_name=%s\n", vm_name );
        retval = -1;
        goto EXIT;
    }

    if ((vm->vm_state == VmState_NULL) || (vm->vm_state == VmState_IN_SHUTTING_DOWN)) {
        Dbg_printf( NODE, ERROR, "invalid vm state=%d\n", vm->vm_state );
        retval = -1;
        goto EXIT;
    }

    if (VmMonitor_shutdown_theater_req( vm->vmmon_addr, node_addr, vm->theater ) < 0) {
        Dbg_printf( NODE, ERROR, "VmMonitor_shutdown_theater_req failed, vmmon_addr=%s, node_addr=%s, theater=%s\n", vm->vmmon_addr, node_addr, vm->theater );
        retval = -1;
        goto EXIT;
    }

EXIT:
    if (retval < 0) {
        vm->vm_state = VmState_NULL;
        if (CosManager_destroy_vm_resp( cos_addr, node_addr, vm_name, -1 /*FAIL*/ ) < 0)
            Dbg_printf( NODE, ERROR, "CosManager_destroy_vm_resp failed\n" );
    }
    else {
        vm->vm_state = VmState_IN_SHUTTING_DOWN;
    }

    return retval;    
}


static
int notify_vm_started( char *vmmon_addr, char *theater )
{
    VM *vm;
    int retval = 0, i;

    Dbg_printf( NODE, INFO, "entered\n" );

    /* TODO: this causes a problem if a multiple VM creation is taking place */
    if ((vm = find_vm_by_state( VmState_IN_CREATION )) == NULL) {
        Dbg_printf( NODE, ERROR, "valid vm management entry not found\n" );
        retval = -1;
        goto EXIT;
    }

    strcpy( vm->vmmon_addr, vmmon_addr );
    strcpy( vm->theater, theater );

    Dbg_printf( NODE, INFO, "VM has successfully started (vmmon_addr=%s, theater=%s)\n", vmmon_addr, theater );

EXIT:
    if (retval < 0) {
        if (CosManager_create_vm_resp( cos_addr, node_addr, NULL, NULL, -1 ) < 0) {
            Dbg_printf( NODE, ERROR, "CosManager_create_vm_resp failed\n" );
        }
    }
    else {
        if (CosManager_create_vm_resp( cos_addr, node_addr, vm->vm_name, vm->theater, 0 ) < 0) {
            Dbg_printf( NODE, ERROR, "CosManager_create_vm_resp failed\n" );
            vm->vm_state = VmState_NULL;
            retval = -1;
        }
        else {
            /* SUCCESS */
            vm->vm_state = VmState_ACTIVE;
        }
    }

    return retval;
}


static
int notify_high_cpu_usage( 
    char *vmmon_addr, 
    double cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    VM *vm;

    Dbg_printf( NODE, INFO, "vmmon_addr=%s\n", vmmon_addr );
    
    if ((vm = find_vm_by_addr( vmmon_addr )) == NULL) {
        Dbg_printf( NODE, ERROR, "valid vm management entry not found\n" );
        return -1;
    }

    if (CosManager_notify_high_cpu_usage( 
            cos_addr, node_addr, vm->vm_name, cpu_usage_history ) < 0) {
        Dbg_printf( NODE, ERROR, "CosManager_notify_high_cpu_usage failed\n" );
        return -1;
    }

    return 0;
}


static
int notify_low_cpu_usage( 
    char *vmmon_addr, 
    double cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    VM *vm;

    Dbg_printf( NODE, INFO, "vmmon_addr=%s\n", vmmon_addr );
    
    if ((vm = find_vm_by_addr( vmmon_addr )) == NULL) {
        Dbg_printf( NODE, ERROR, "valid vm management entry not found\n" );
        return -1;
    }

    if (CosManager_notify_low_cpu_usage( 
            cos_addr, node_addr, vm->vm_name, cpu_usage_history ) < 0) {
        Dbg_printf( NODE, ERROR, "CosManager_notify_low_cpu_usage failed\n" );
        return -1;
    }

    return 0;
}


static
int shutdown_theater_resp(
    char *vmmon_addr, 
    int result )
{
    VM *vm = NULL;
    int retval = 0, cos__resp = 0;

    Dbg_printf( NODE, INFO, "vmmon_addr=%s\n", vmmon_addr );

    if ((vm = find_vm_by_addr( vmmon_addr )) == NULL) {
        Dbg_printf( NODE, ERROR, "valid vm management entry not found, vmmon_addr=%s\n", vmmon_addr );
        retval = -1;
        cos__resp = -1;
        goto EXIT;
    }

    if (vm->vm_state != VmState_IN_SHUTTING_DOWN) {
        Dbg_printf( NODE, ERROR, "invalid vm state=%d\n", vm->vm_state );
        retval = -1;
        cos__resp = -1;
        goto EXIT;
    }

#if 0
    if (exec_xm_command( "shutdown", vm->vm_name, NULL, NULL, NULL ) < 0) {
        Dbg_printf( NODE, ERROR, "xm shutdown vm=%s failed \n", vm->vm_name );
        retval = -1;
        cos__resp = -1;
        goto EXIT;
    }
#else
    Dbg_printf( NODE, ERROR, "VM %s SHUTDOWN SKIPPED at %lf!!!\n", vm->vm_name, gettimeofday_msec() );
#endif

EXIT:
    if (CosManager_destroy_vm_resp( cos_addr, node_addr, vm->vm_name, cos__resp ) < 0) {
        Dbg_printf( NODE, ERROR, "CosManager_destroy_vm_resp failed\n" );
        retval = -1;
    }

    /* Complete reset */
    /* vm->vm_state = VmState_NULL; */
    memset( (void*)vm, 0x00, sizeof( VM ) );

    return retval;
}


int main( int argc, char *argv[] )
{
    int buflen, retval;
    char buf[MAX_RECVBUF_LEN], eth0_addr[16];
    NodeMessage *msg;


    if (argc == 1) {
        listen_port = DEFAULT_LISTEN_PORT;
    }
    else if (argc == 2) {
        listen_port = atoi( argv[1] );
    }
    else {
        Dbg_printf( NODE, ERROR, "Usage: ./NodeController [listen_port]\n" );
        exit( 1 );
    }

    /* setup a handler for Ctrl-C */
    if ( SIG_ERR == signal ( SIGINT, sigcatch ) ) {
        Dbg_printf( NODE, ERROR, "Failed to set signal handler\n" );
        exit( 1 );
    }

    /* create a socket and start listening */
    if ((sock = Socket_create()) < 0) {
        Dbg_printf( NODE, ERROR, "Failed to create the socket\n" );
        exit( 1 );
    }
    if (Socket_bind( sock, listen_port ) < 0) {
        Dbg_printf( NODE, ERROR, "Failed to bind the socket\n" );
        exit( 1 );
    }
    if (Socket_listen( sock ) < 0) {
        Dbg_printf( NODE, ERROR, "Failed to listen the socket\n" );
        exit( 1 );
    }

    /* Initialization */
    init_vm_array( vm_array );
    get_netif_addr( "eth0", eth0_addr );
    sprintf( node_addr, "%s:%d", eth0_addr, listen_port );

    while( 1 ) {
        Dbg_printf( NODE, DEBUG, "NODE MANAGER LISTENING %s\n", node_addr );

        /* accepting data */
        buflen = MAX_RECVBUF_LEN;
        newsock = Socket_accept( sock );
        Socket_set_blocking( newsock );
        Dbg_printf( NODE, DEBUG, "Request accepted\n" );
        Socket_receive_data( newsock, buf, &buflen ); /* buf is NULL teminated when returned */
        if (buflen == 0)
            continue;

        msg = node_parse_msg( buf, buflen );
        if (msg == NULL) {
            Dbg_printf( NODE, ERROR, "node_parse_msg failed\n" );
            continue;
        }

        Dbg_printf( NODE, API, "%s message received from %s\n", node_msgid2str( msg->msgid ), msg->return_addr );

        switch (msg->msgid) {
        case NodeMessageID_CREATE_VM_REQ:
            strcpy( cos_addr, msg->return_addr );
            #if 0
            create_vm_req( 
                msg->create_vm_req_msg.vmcfg_file,
                msg->create_vm_req_msg.peer_theaters );
            #else
            start_vm_terminal( eth0_addr );
            #endif
            break;

        case NodeMessageID_DESTROY_VM_REQ:
            destroy_vm_req(
                msg->destroy_vm_req_msg.vm_name );
            break;

        case NodeMessageID_NOTIFY_VM_STARTED:
            notify_vm_started( 
                msg->notify_vm_started_msg.vmmon_addr, 
                msg->notify_vm_started_msg.theater );
            break;

        case NodeMessageID_NOTIFY_HIGH_CPU_USAGE:
            notify_high_cpu_usage(
                msg->notify_cpu_usage_msg.vmmon_addr,
                msg->notify_cpu_usage_msg.cpu_usage_history );
            break;

        case NodeMessageID_NOTIFY_LOW_CPU_USAGE:
            notify_low_cpu_usage(
                msg->notify_cpu_usage_msg.vmmon_addr,
                msg->notify_cpu_usage_msg.cpu_usage_history );
            break;

        case NodeMessageID_SHUTDOWN_THEATER_RESP:
            shutdown_theater_resp(
                msg->shutdown_theater_resp_msg.vmmon_addr,
                msg->shutdown_theater_resp_msg.result );
            break;

        default:
            break;
        }

        node_dealloc_msg( msg );
        Socket_close( newsock );
    }
}
