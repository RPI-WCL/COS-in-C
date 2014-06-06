#ifndef _DEBUG_PRINT_H
#define _DEBUG_PRINT_H

#include <stdio.h>

extern int dbg_level;

#define DEBUG   0x00000001
#define INFO    0x00000002
#define WARN    0x00000004
#define ERROR   0x00000008
#define FATAL   0x00000010
#define GRAPH   0x00000020
#define API     0x00000040
#define ALL     0xFFFFFFFF

#define NODE    0x00000001
#define COS     0x00000002
#define COMMON  0x00000004
#define VMMON   0x00000008
#define PUBCLOUD 0x00000010
#define TEST    0x00000020

#define NODE_MODULE     "Node"
#define COS_MODULE      "COS"
#define COMMON_MODULE   "Common"
#define VMMON_MODULE    "VMMon"
#define PUBCLOUD_MODULE "PubCloud"
#define UNKNOWN_MODULE  "Unknown"
#define TEST_MODULE     "Test"


// #define Dbg_printf(level, ... )                                       \
//     if (level & dbg_level) printf( "%s:%s:%d, ", __FILE__, __func__, __LINE__, __VA_ARGS__ )
// #define Dbg_printf(level, ... )                                       \
//     if (level & dbg_level) printf("[%s:%s:%d] ",__FILE__,__func__,__LINE__);printf(__VA_ARGS__)

#define Dbg_printf(module,level, ... )                                     \
    if (level & dbg_level) { \
        if (level & ERROR) printf( "\x1b[41m" );    \
        else if (level & WARN) printf( "\x1b[41m" );                  \
        else if (level & FATAL) printf( "\x1b[41m" );                  \
        else if (level & API) printf( "\x1b[46m" );                    \
        if (module & NODE) {printf("[%s] %s:%s:%d, ",NODE_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout);} \
        else if (module & COS) {printf("[%s] %s:%s:%d, ",COS_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout);} \
        else if (module & COMMON) {printf("[%s] %s:%s:%d, ",COMMON_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout);} \
        else if (module & VMMON) {printf("[%s] %s:%s:%d, ",VMMON_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout);} \
        else if (module & PUBCLOUD) {printf("[%s] %s:%s:%d, ",PUBCLOUD_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout);} \
        else if (module & TEST) {printf("[%s] %s:%s:%d, ",TEST_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout);} \
        else {printf("[%s] %s:%s:%d, ",UNKNOWN_MODULE,__FILE__,__func__,__LINE__);printf(__VA_ARGS__);fflush(stdout); }    \
        printf( "\x1b[0m" ); }


#endif /* #ifndef _DEBUG_PRINT_H */
