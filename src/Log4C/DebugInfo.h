#ifndef __DEBUGINFO_HH
#define __DEBUGINFO_HH

/*
*date:   2018/8/29
*function: read global parmeter 'LOG_LEVEL' to control log print
*/

#include<stdio.h>
#include<stdlib.h>

#define ERR 1
#define WARN 2
#define DEBUG 4
#define UTEST 8

#define ISESOL_ERR if(0x0001 & dbg_level()) DBG_PRINT
#define ISESOL_WARN if(0x0002 & dbg_level()) DBG_PRINT
#define ISESOL_DEBUG if(0x0004 & dbg_level()) DBG_PRINT
#define ISESOL_UTEST if(0x0008 & dbg_level()) DBG_PRINT

#define DBG_PRINT (printf("%s:%u %s:%s:\t", __FILE__, __LINE__, __DATE__, __TIME__), printf)

static int dbg_level()
{
	char* level = getenv("LOG_LEVEL");
	if(level != NULL) return atoi(level); else return 0;
}

#endif
