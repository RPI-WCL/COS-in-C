#ifndef _PARSE_MSG_H
#define _PARSE_MSG_H

#define MAX_PARSED_MSG   10

/* Expected data order in a msg */
/* First line  : XX\n ; the number of data entries */
/* Second line : MSGID,PARAM1,PARAM2,.... ; data entries */

int ParseMsg_parse( 
    char        *buf,                           /* IN */
    int         buflen,                         /* IN */
    char        *parsed_msg[MAX_PARSED_MSG],    /* OUT */
    int         *parsed_params_num );           /* OUT */

#endif /* #ifndef _PARSE_MSG_H */
