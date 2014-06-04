#include <string.h>
#include "ParseMsg.h"
#include "DebugPrint.h"

/* Expected data order in a msg */
/* First line  : XX\n ; the number of data entries */
/* Second line : MSGID,PARAM1,PARAM2,.... ; data entries */

int ParseMsg_parse( 
    char        *buf,                           /* IN */
    int         buflen,                         /* IN */
    char        *parsed_msg[MAX_PARSED_MSG],    /* OUT */
    int         *parsed_params_num )            /* OUT */
{
    int i = 0, data_num;
    char *cr, *str;

	if ((cr = strchr( buf, '\n' )) == NULL) {
		/* CR not found */
		return -1;
	}

	/* CR found */
	*cr = '\0';
    data_num = atoi( buf );
    if (MAX_PARSED_MSG < data_num) {
        Dbg_printf( COMMON, ERROR, "ParseMsg_parse, data_num(%d) exceeded MAX_PARSED_MSG(%d)\n", 
                    data_num, MAX_PARSED_MSG );
        return -1;
    }

	str = cr + 1;
    while (str = strtok( str, "," ))
    {
        parsed_msg[i++] = str;
        str = NULL;
    }

    if (i < data_num) {
        Dbg_printf( COMMON, ERROR, "ParseMsg_parse, not enough data to parse(%d/%d)\n", i, data_num );
        return -1;
    }
    
    *parsed_params_num = i;

	return 0;
}

