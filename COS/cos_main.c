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
#include <errno.h>
#include <time.h>
#include <sys/times.h>


#include "Socket.h"
#include "DebugPrint.h"
#include "cos_msg.h"

#define DEFAULT_LISTEN_PORT     5000
#define DEFAULT_SCRIPT_EXEC_PORT     6000
#define DEFAULT_NODELIST_FILE     "nodelist.txt"
#define MAX_RECVBUF_LEN         512
#define NODE_ARRAY_LEN          32
#define VM_CFG_FILE_PATH        "/etc/xen/cos/"
#define VM_NAME_PREFIX          "cos"
#define THEATER_FILE            "./theater.txt"
#define HIGH_CPU_USAGE_CHECK_TIME_THRESHOLD     10000   /* msec */

typedef enum {
    NodeState_NULL,
    NodeState_IN_VM_CREATION,
    NodeState_IN_VM_DESTRUCTION,
    NodeState_VM_ACTIVE,
} NodeState;

typedef struct {
    /* NodeManager management */
    int id;
    NodeState state;
    char addr[24];

    /* VM management */
    char vm_name[16];   /* one VM per Node */
    char theater[24];   /* one theater per VM */

    /* CPU utilization flag*/
    int high_cpu_usage;
    double time_high_cpu_usage_notified;
} Node;

static int sock = -1;
static int newsock = -1;
static int listen_port = -1;
static Node node_array[NODE_ARRAY_LEN];
static int num_node = -1;
static int num_active_vm = 0;
static char peer_theaters_memory[MAX_THEATERS * THEATER_ADDR_LEN];
char cos_addr[24];

int dbg_level = ALL;

static
void sigcatch( int sig )
{
    Dbg_printf( COS, INFO, "Catched Ctrl-C signal, closing the sockets\n" );
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
int init_node_array( char *nodelist_file,  Node node_array[NODE_ARRAY_LEN] )
{
    int i;
    FILE *fp;

    /* state will be NodeState_NULL */
    memset( (void*)node_array, 0x00, sizeof( node_array ) );

    if ((fp = fopen( nodelist_file, "r" )) == NULL) {
        Dbg_printf( COS, ERROR, "nodelist_file:%s not found\n", nodelist_file );
        return -1;
    }

    i = 0;
    while (fgets( node_array[i].addr, 24, fp) != NULL) {
        node_array[i].id = i;
        i++;
    }

    num_node = i;

    fclose( fp );

    return 0;
}


static
Node* find_node_by_vm_name( char *vm_name )
{
    int i;
    Node *node = NULL;
    
    for (i = 0; i < num_node; i++) {
        if (strcmp( node_array[i].vm_name, vm_name ) == 0) {
            node = &node_array[i];
            break;
        }
    }
            
    return node;
}

static
Node* find_node_by_state( NodeState state )
{
    int i;
    Node *node = NULL;
    
    for (i = 0; i < num_node; i++) {
        if (node_array[i].state == state ) {
            node = &node_array[i];
            break;
        }
    }
            
    return node;
}


static
void collect_active_theaters( char *peer_theaters[MAX_THEATERS] )
{
    int i, active_peer;

    active_peer = 0;
    memset( (void*)peer_theaters_memory, 0x00, sizeof(peer_theaters_memory) );
    /* for (i = 0; i < MAX_THEATERS; i++) */
    /*     peer_theaters[i] = NULL; */

    /* The above code does not work, so manualy set NULL */
    peer_theaters[0] = NULL;
    peer_theaters[1] = NULL;
    peer_theaters[2] = NULL;
    peer_theaters[3] = NULL;

    for (i = 0; i < num_node; i++) {
        if (node_array[i].state == NodeState_VM_ACTIVE) {
            strcpy( &peer_theaters_memory[active_peer * THEATER_ADDR_LEN], 
                    node_array[i].theater );
            peer_theaters[active_peer] = &peer_theaters_memory[active_peer * THEATER_ADDR_LEN];
            active_peer++;
        }
    }
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
        Dbg_printf( COS, FATAL, "exec_command(), fork failed\n" );
        exit( 1 );
    }

    if (pid == 0) {
        /* CHILD PROCESS */

        Dbg_printf( COS, DEBUG, "params={%s,%s,%s,%s,%s,%s}\n", 
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
                    Dbg_printf( COS, INFO, "Timeout, killing child\n" );
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
int create_vm_req( void )
{
    Node *node;
    char vmcfg_file[32];
    char *peer_theaters[MAX_THEATERS];

    Dbg_printf( COS, INFO, "entered\n" );

    node = find_node_by_state( NodeState_NULL );
    if (node == NULL) {
        Dbg_printf( COS, INFO, "no nodes available\n" );
        return -1;
    }

    sprintf( vmcfg_file, "%s%s%02d.cfg", VM_CFG_FILE_PATH, VM_NAME_PREFIX, node->id );

    collect_active_theaters( peer_theaters );

    if (NodeManager_create_vm_req( node->addr, cos_addr, vmcfg_file, peer_theaters ) < 0 ) {
        Dbg_printf( COS, ERROR, "create_vm_req failed, addr=%s, return_cos_addr=%s, vmcfg_file=%s\n", node->addr, cos_addr, vmcfg_file );
        return -1;
    }

    node->state = NodeState_IN_VM_CREATION;

    return 0;
}


static
int get_num_active_theaters( void )
{
    Dbg_printf( COS, DEBUG, "entered\n" );

    int i, num_active_theaters = 0;

    for (i = 0; i < num_node; i++) {
        if (node_array[i].state == NodeState_VM_ACTIVE)
            num_active_theaters++;
    }
    
    return num_active_theaters;
}


static
int connect_peer_theaters( void )
{
    char *peer_theaters[MAX_THEATERS];
    FILE *fp;
    int i;
    
    Dbg_printf( COS, DEBUG, "entered\n" );

    collect_active_theaters( peer_theaters );

    /* create a theater file */
    if ((fp = fopen( THEATER_FILE, "w" )) == NULL) {
        Dbg_printf( COS, ERROR, "fopen for %s failed\n", THEATER_FILE );
        return -1;
    }
    
    i = 0;
    while (peer_theaters[i]) {
        fprintf( fp, "%s\n", peer_theaters[i] );
        i++;
    }

    fclose( fp );

#if 1
    Dbg_printf( COS, ERROR, "Theaters connected at %lf\n", gettimeofday_msec() );

    if (exec_command( "/usr/bin/java", "java", "-Dnetif=eth0",
                      "src.testing.reachability.Full", THEATER_FILE, NULL, 5000 ) < 0) {
        Dbg_printf( COS, ERROR, "exec_command failed\n" );
        return -1;
    }
#else
    if (request_script_exec() < 0) {
        Dbg_printf( COS, ERROR, "request_script_exec failed\n" );
        return -1;
    }

    /* if (JNI_call_static_method( jvm_options, 2, */
    /*                             "src/testing/reachability/Full",  */
    /*                             "main", "([Ljava/lang/String;)V" ) < 0) { */
    /*     Dbg_printf( COS, ERROR, "JNI call failed\n" ); */
    /*     return -1; */
    /* } */

#endif

    return 0;
}


static
int create_vm_resp( char *vm_name, char *new_theater, int result )
{
    Node *node;

    Dbg_printf( COS, INFO, "vm_name=%s, new_theater=%s, result=%d\n",
                vm_name, new_theater, result );

    node = find_node_by_state( NodeState_IN_VM_CREATION );
    if (!node) {
        Dbg_printf( COS, ERROR, "no node with the state=%d found in the Node list\n", NodeState_IN_VM_CREATION );
        return -1;
    }

    if (result < 0) {
        Dbg_printf( COS, ERROR, "create_vm_resp returned result=%d\n", result );
        node->state = NodeState_NULL;
        return -1;
    }

    strcpy( node->vm_name, vm_name );
    strcpy( node->theater, new_theater );

    node->state = NodeState_VM_ACTIVE;
    num_active_vm++;

    if (1 < get_num_active_theaters()) 
        connect_peer_theaters();

    return 0;
}

static
int check_all_nodes_cpu_usage( void )
{
    int i, retval = 1;
    double current_time;
    Node *node;

    current_time = gettimeofday_msec();
    for (i = 0; i < num_node; i++) {
        node = &node_array[i];
        if (node->state == NodeState_IN_VM_CREATION) {
            /* there is new VM coming */
            node->high_cpu_usage = 0;
            node->time_high_cpu_usage_notified = 0;
            retval = 0;
            break;
        }
        else    
        if (node->state == NodeState_VM_ACTIVE) {
            if (!node->high_cpu_usage ||
                ((node->time_high_cpu_usage_notified + HIGH_CPU_USAGE_CHECK_TIME_THRESHOLD) < current_time)) {
                /* there may be an under utilized VM */
                node->high_cpu_usage = 0;
                node->time_high_cpu_usage_notified = 0;
                retval = 0;
                break;
            }
        }
    }
            
    return retval;
}


static
int notify_high_cpu_usage( char *vm_name, double cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    Node *node;
    int retval = 0;

    Dbg_printf( COS, INFO, "vm_name=%s\n", vm_name );

    node = find_node_by_vm_name( vm_name );
    if (!node) {
        Dbg_printf( COS, ERROR, "vm_name=%s not found in the Node list\n", vm_name );
        return -1;
    }

    if (node->state != NodeState_VM_ACTIVE) {
        Dbg_printf( COS, ERROR, "invalid state(%d)\n", node->state );
        return -1;
    }

    node->high_cpu_usage = 1;
    node->time_high_cpu_usage_notified = gettimeofday_msec();
    
    if (check_all_nodes_cpu_usage()) {
        /* all nodes are highly utilized, so create a new VM */
        retval = create_vm_req();
    }
    else {
        Dbg_printf( COS, INFO, "not all nodes are highly utilized\n" );
        retval = 0;
    }

    return retval;
}


static
int notify_low_cpu_usage( char *vm_name, double cpu_usage_history[CPU_USAGE_HISTORY_LEN] )
{
    Node *node;

    Dbg_printf( COS, INFO, "vm_name=%s\n", vm_name );

    node = find_node_by_vm_name( vm_name );
    if (!node) {
        Dbg_printf( COS, ERROR, "vm_name=%s not found in the Node list\n", vm_name );
        return -1;
    }

    if (node->state != NodeState_VM_ACTIVE) {
        Dbg_printf( COS, ERROR, "invalid state(%d)\n", node->state );
        return -1;
    }

    /* if (node->id == 0) { */
    /*     Dbg_printf( COS, INFO, "notified Node has the primary ID(0), IGNORE the low cpu notification!!!\n" ); */
    /*     return 0; */
    /* } */

    node->high_cpu_usage = 0;

    if (NodeManager_destroy_vm_req( 
            node->addr, cos_addr, node->vm_name ) < 0) {
        Dbg_printf( COS, ERROR, "destroy_vm_req failed\n" );
        return -1;
    } 
    
    node->state = NodeState_IN_VM_DESTRUCTION;

    return 0;
}


static
int destroy_vm_resp( char *vm_name, int result )
{
    Node *node;
    int id;

    Dbg_printf( COS, INFO, "vm_name=%s, result=%d\n", vm_name, result );

    node = find_node_by_vm_name( vm_name );
    if (!node) {
        Dbg_printf( COS, ERROR, "no node with the VM name=%s found\n", vm_name );
        return -1;
    }

    if (result < 0) {
        Dbg_printf( COS, ERROR, "destroy_vm_resp returned result=%d\n", result );
        node->state = NodeState_NULL;
        return -1;
    }

    id = node->id;
    memset( (void*)node, 0x00, sizeof( node_array ) );
    node->id = id;        /* TODO: id recovery needed ???? */

    num_active_vm--;

    return 0;
}


static
int launch_terminal_req( char *cmd )
{
    Dbg_printf( COS, INFO, "cmd=%s\n", cmd );
    system( cmd );
}

int main( int argc, char *argv[] )
{
    int buflen, retval;
    char buf[MAX_RECVBUF_LEN], *nodelist_file, netif_addr[16], *netif;
    CosMessage *msg;

    if (argc == 1) {
        listen_port = DEFAULT_LISTEN_PORT;
        nodelist_file = DEFAULT_NODELIST_FILE;
    }
    else if (argc == 2) {
        listen_port = atoi( argv[1] );
        nodelist_file = DEFAULT_NODELIST_FILE;
    }
    else if (argc == 3) {
        listen_port = atoi( argv[1] );
        nodelist_file = argv[2];
    }
    else {
        Dbg_printf( COS, ERROR, "Usage: ./ClusterController [listen_port] [nodelist_file]\n" );
        exit( 1 );
    }

    /* setup a handler for Ctrl-C */
    if ( SIG_ERR == signal ( SIGINT, sigcatch ) ) {
        Dbg_printf( COS, ERROR, "Failed to set signal handler\n" );
        exit( 1 );
    }

    /* create a socket and start listening */
    if ((sock = Socket_create()) < 0) {
        Dbg_printf( COS, ERROR, "Failed to create the socket\n" );
        exit( 1 );
    }
    if (Socket_bind( sock, listen_port ) < 0) {
        Dbg_printf( COS, ERROR, "Failed to bind the socket\n" );
        exit( 1 );
    }
    if (Socket_listen( sock ) < 0) {
        Dbg_printf( COS, ERROR, "Failed to listen the socket\n" );
        exit( 1 );
    }

    /* initialization */
    init_node_array( nodelist_file, node_array );
    if ((netif = getenv( "netif" )) == NULL)
        get_netif_addr( "eth0", netif_addr );
    else 
        get_netif_addr( netif, netif_addr );
    sprintf( cos_addr, "%s:%d", netif_addr, listen_port );

    /* request a VM creation to the default NODE */
    create_vm_req();

    while( 1 ) {
        Dbg_printf( COS, DEBUG, "COS MANAGER LISTENING %s\n", cos_addr );

        /* accepting data */
        buflen = MAX_RECVBUF_LEN;
        newsock = Socket_accept( sock );
        /* Socket_set_blocking( newsock ); */
        Dbg_printf( COS, DEBUG, "Request accepted, newsock=%d\n", newsock );

        if (newsock < 0) {
            Dbg_printf( COS, ERROR, "newsock=%d, errno=%s(%d)\n", 
                        newsock, strerror(errno), errno );
            continue;
        }

        Socket_receive_data( newsock, buf, &buflen ); /* buf is NULL teminated when returned */
        if (buflen == 0)
            continue;

        msg = cos_parse_msg( buf, buflen );
        if (msg == NULL) {
            Dbg_printf( COS, ERROR, "cos_parse_msg failed\n" );
            continue;
        }

        Dbg_printf( COS, API, "%s message received from %s\n", cos_msgid2str( msg->msgid ), msg->return_addr );

        switch (msg->msgid) {
        case CosMessageID_CREATE_VM_RESP:
            create_vm_resp( 
                msg->create_vm_resp_msg.vm_name, 
                msg->create_vm_resp_msg.new_theater, 
                msg->create_vm_resp_msg.result );
            break;

        case CosMessageID_NOTIFY_HIGH_CPU_USAGE:
            if (num_active_vm == num_node) {
                Dbg_printf( COS, WARN, "The number of active VMs has reached limit(%d), IGNORE the high cpu notifiication!!!\n", num_node );
                break;
            }
            notify_high_cpu_usage(
                msg->notify_cpu_usage_msg.vm_name, 
                msg->notify_cpu_usage_msg.cpu_usage_history );
            break;

        case CosMessageID_NOTIFY_LOW_CPU_USAGE:
            if ((num_active_vm == 1) || (num_active_vm == 0)) {
                Dbg_printf( COS, WARN, "There is %d VM, IGNORE the low cpu notiifcation!!!\n", num_active_vm );
                break;
            }
            notify_low_cpu_usage(
                msg->notify_cpu_usage_msg.vm_name, 
                msg->notify_cpu_usage_msg.cpu_usage_history );
            break;

        case CosMessageID_DESTROY_VM_RESP:
            destroy_vm_resp( 
                msg->create_vm_resp_msg.vm_name, 
                msg->create_vm_resp_msg.result );
            break;

        case CosMessageID_LAUNCH_TERMINAL_REQ:
            launch_terminal_req(
                msg->launch_term_req_msg.cmd );
            break;

        case CosMessageID_TEST:
            Dbg_printf( COS, INFO, "data=%s\n", msg->test_msg.data );
            break;

        default:
            break;
        }

        cos_dealloc_msg( msg );
        Socket_close( newsock );
    }
}
