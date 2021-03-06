/*****************************************************************************

 @(#) File: src/test/test-mtp.c

 -----------------------------------------------------------------------------

 Copyright (c) 2008-2015  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 Unauthorized distribution or duplication is prohibited.

 This software and related documentation is protected by copyright and
 distributed under licenses restricting its use, copying, distribution and
 decompilation.  No part of this software or related documentation may be
 reproduced in any form by any means without the prior written authorization
 of the copyright holder, and licensors, if any.

 The recipient of this document, by its retention and use, warrants that the
 recipient will protect this information and keep it confidential, and will
 not disclose the information contained in this document without the written
 permission of its owner.

 The author reserves the right to revise this software and documentation for
 any reason, including but not limited to, conformity with standards
 promulgated by various agencies, utilization of advances in the state of the
 technical arts, or the reflection of changes in the design of any techniques,
 or procedures embodied, described, or referred to herein.  The author is
 under no obligation to provide any feature listed herein.

 -----------------------------------------------------------------------------

 As an exception to the above, this software may be distributed under the GNU
 Affero General Public License (AGPL) Version 3, so long as the software is
 distributed with, and only used for the testing of, OpenSS7 modules, drivers,
 and libraries.

 -----------------------------------------------------------------------------

 U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on
 behalf of the U.S. Government ("Government"), the following provisions apply
 to you.  If the Software is supplied by the Department of Defense ("DoD"), it
 is classified as "Commercial Computer Software" under paragraph 252.227-7014
 of the DoD Supplement to the Federal Acquisition Regulations ("DFARS") (or any
 successor regulations) and the Government is acquiring only the license rights
 granted herein (the license rights customarily provided to non-Government
 users).  If the Software is supplied to any unit or agency of the Government
 other than DoD, it is classified as "Restricted Computer Software" and the
 Government's rights in the Software are defined in paragraph 52.227-19 of the
 Federal Acquisition Regulations ("FAR") (or any successor regulations) or, in
 the cases of NASA, in paragraph 18.52.227-86 of the NASA Supplement to the FAR
 (or any successor regulations).

 -----------------------------------------------------------------------------

 Commercial licensing and support of this software is available from OpenSS7
 Corporation at a fee.  See http://www.openss7.com/

 *****************************************************************************/

static char const ident[] = "src/test/test-mtp.c (" PACKAGE_ENVR ") " PACKAGE_DATE;

/*
 *  This is a ferry-clip Q.782 conformance test program for testing the
 *  OpenSS7 MTP multiplexing driver.
 *
 *  GENERAL OPERATION:
 *
 *  The test program opens a management stream to the MTP multiplexing
 *  driver, configures the driver for single signalling point, single
 *  signalling relation, and single trunk group.  It then opens a pipe and
 *  links one end of the pipe underneath the MTP driver as the MTP Level 2
 *  stream.
 *
 *  The test program opens an MTP stream and feeds MTP primitive to and
 *  from that stream.  On the open end of the pipe it feeds MTP Level 2
 *  primitives into the bottom of the MTP multiplexing driver.  This is the
 *  ferry-clip test arrangement as follows:
 *
 *
 *                                  USER SPACE                              
 *                                 TEST PROGRAM                             
 *
 *             ___________________________________________________          
 *                    \   /  |                  \   /  |                    
 *                     \ /   |                   \ /   |                    
 *                      |   / \                   |    |                    
 *             _________|__/___\_______           |    |                    
 *            |                        |          |    |                    
 *            |                        |          |    |                    
 *            |                        |          |    |                    
 *            |           MTP          |          |    |                    
 *            |                        |          |    |                    
 *            |                        |          |    |                    
 *            |________________________|          |    |                    
 *                    \   /  |                    |    |                    
 *                     \ /   |                    |    |                    
 *                      |    |                    |    |                    
 *                      |   / \                   |   / \                   
 *             _________|__/___\__________________|__/___\________          
 *            |                                                   |         
 *            |                                                   |         
 *            |                        PIPE                       |         
 *            |                                                   |         
 *            |___________________________________________________|         
 */

#include <sys/types.h>
#include <stropts.h>
#include <stdlib.h>

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#else
# ifdef HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/uio.h>
#include <time.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <ss7/lmi.h>
#include <ss7/lmi_ioctl.h>
#include <ss7/sdli.h>
#include <ss7/sdli_ioctl.h>
#if 0
#include <ss7/devi.h>
#include <ss7/devi_ioctl.h>
#endif
#include <ss7/mtpi.h>
#include <ss7/mtpi_ioctl.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#   define __constant_ntohl(x)	(x)
#   define __constant_ntohs(x)	(x)
#   define __constant_htonl(x)	(x)
#   define __constant_htons(x)	(x)
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN
#   define __constant_ntohl(x)	__bswap_constant_32(x)
#   define __constant_ntohs(x)	__bswap_constant_16(x)
#   define __constant_htonl(x)	__bswap_constant_32(x)
#   define __constant_htons(x)	__bswap_constant_16(x)
#endif
#endif



/*
 *  -------------------------------------------------------------------------
 *
 *  Configuration
 *
 *  -------------------------------------------------------------------------
 */

static const char *lpkgname = "SS7 MTP Level 3";

/* static const char *spkgname = "strss7"; */
static const char *lstdname = "Q.704/ANSI T1.111.4/JQ.704";
static const char *sstdname = "Q.782";
static const char *shortname = "MTP3";
static char devname[256] = "/dev/streams/clone/mtp";

static int repeat_verbose = 0;
static int repeat_on_success = 0;
static int repeat_on_failure = 0;
static int exit_on_failure = 0;

static int verbose = 1;

static int client_exec = 0;		/* execute client side */
static int server_exec = 0;		/* execute server side */

static int show_msg = 0;
static int show_acks = 0;
static int show_timeout = 0;
static int show_fisus = 1;
static int show_msus = 1;

//static int show_data = 1;

static int last_prim = 0;
static int last_event = 0;
static int last_errno = 0;
static int last_retval = 0;
static int last_prio = 0;

#define TEST_PROTOCOL 132

#define CHILD_PTU   0
#define CHILD_IUT   1

int test_fd[3] = { 0, 0, 0 };
uint32_t bsn[3] = { 0, 0, 0 };
uint32_t fsn[3] = { 0, 0, 0 };
uint8_t fib[3] = { 0, 0, 0, };
uint8_t bib[3] = { 0, 0, 0, };
uint8_t li[3] = { 0, 0, 0, };
uint8_t sio[3] = { 0, 0, 0, };

static int iut_connects = 1;

#define MSU_LEN 35
static int msu_len = MSU_LEN;

/*
   some globals for compressing events 
 */
static int oldact = 0;			/* previous action */
static int cntact = 0;			/* repeats of previous action */
static int oldevt = 0;
static int cntevt = 0;

static int count = 0;
static int tries = 0;

#define BUFSIZE 32*4096

#define SHORT_WAIT	  20	// 100 // 10
#define NORMAL_WAIT	 100	// 500 // 100
#define LONG_WAIT	 500	// 5000 // 500
#define LONGER_WAIT	1000	// 10000 // 5000
#define LONGEST_WAIT	5000	// 20000 // 10000
#define TEST_DURATION	20000
#define INFINITE_WAIT	-1

static ulong test_duration = TEST_DURATION;	/* wait on other side */

ulong seq[10] = { 0, };
ulong tok[10] = { 0, };
ulong tsn[10] = { 0, };
ulong sid[10] = { 0, };
ulong ssn[10] = { 0, };
ulong ppi[10] = { 0, };
ulong exc[10] = { 0, };

char cbuf[BUFSIZE];
char dbuf[BUFSIZE];

struct strbuf ctrl = { BUFSIZE, -1, cbuf };
struct strbuf data = { BUFSIZE, -1, dbuf };

static int test_pflags = MSG_BAND;	/* MSG_BAND | MSG_HIPRI */
static int test_pband = 0;
static int test_gflags = 0;		/* MSG_BAND | MSG_HIPRI */
static int test_gband = 0;
static int test_timout = 200;

static unsigned short addrs[3][1] = {
	{(PTU_TEST_SLOT << 12) | (PTU_TEST_SPAN << 8) | (PTU_TEST_CHAN << 0)},
	{(IUT_TEST_SLOT << 12) | (IUT_TEST_SPAN << 8) | (IUT_TEST_CHAN << 0)},
	{(IUT_TEST_SLOT << 12) | (IUT_TEST_SPAN << 8) | (IUT_TEST_CHAN << 0)}
};
static int anums[3] = { 1, 1, 1 };
static unsigned short *ADDR_buffer = NULL;
static size_t ADDR_length = sizeof(unsigned short);

struct strfdinsert fdi = {
	{BUFSIZE, 0, cbuf},
	{BUFSIZE, 0, dbuf},
	0,
	0,
	0
};
int flags = 0;

int dummy = 0;

typedef unsigned short ppa_t;

struct timeval when;

/*
 *  -------------------------------------------------------------------------
 *
 *  Events and Actions
 *
 *  -------------------------------------------------------------------------
 */
enum {
	__EVENT_EOF = -7, __EVENT_NO_MSG = -6, __EVENT_TIMEOUT = -5, __EVENT_UNKNOWN = -4,
	__RESULT_DECODE_ERROR = -3, __RESULT_SCRIPT_ERROR = -2,
	__RESULT_INCONCLUSIVE = -1, __RESULT_SUCCESS = 0, __RESULT_FAILURE = 1,
	__RESULT_NOTAPPL = 3, __RESULT_SKIPPED = 77,
};

/*
 *  -------------------------------------------------------------------------
 */

int show = 1;

enum {
	__TEST_CONN_REQ = 100, __TEST_CONN_RES, __TEST_DISCON_REQ,
	__TEST_DATA_REQ, __TEST_EXDATA_REQ, __TEST_INFO_REQ, __TEST_BIND_REQ,
	__TEST_UNBIND_REQ, __TEST_UNITDATA_REQ, __TEST_OPTMGMT_REQ,
	__TEST_ORDREL_REQ, __TEST_OPTDATA_REQ, __TEST_ADDR_REQ,
	__TEST_CAPABILITY_REQ, __TEST_CONN_IND, __TEST_CONN_CON,
	__TEST_DISCON_IND, __TEST_DATA_IND, __TEST_EXDATA_IND,
	__TEST_INFO_ACK, __TEST_BIND_ACK, __TEST_ERROR_ACK, __TEST_OK_ACK,
	__TEST_UNITDATA_IND, __TEST_UDERROR_IND, __TEST_OPTMGMT_ACK,
	__TEST_ORDREL_IND, __TEST_NRM_OPTDATA_IND, __TEST_EXP_OPTDATA_IND,
	__TEST_ADDR_ACK, __TEST_CAPABILITY_ACK, __TEST_WRITE, __TEST_WRITEV,
	__TEST_PUTMSG_DATA, __TEST_PUTPMSG_DATA, __TEST_PUSH, __TEST_POP,
	__TEST_READ, __TEST_READV, __TEST_GETMSG, __TEST_GETPMSG,
	__TEST_DATA,
	__TEST_DATACK_REQ, __TEST_DATACK_IND, __TEST_RESET_REQ,
	__TEST_RESET_IND, __TEST_RESET_RES, __TEST_RESET_CON,
	__TEST_O_TI_GETINFO, __TEST_O_TI_OPTMGMT, __TEST_O_TI_BIND,
	__TEST_O_TI_UNBIND,
	__TEST__O_TI_GETINFO, __TEST__O_TI_OPTMGMT, __TEST__O_TI_BIND,
	__TEST__O_TI_UNBIND, __TEST__O_TI_GETMYNAME, __TEST__O_TI_GETPEERNAME,
	__TEST__O_TI_XTI_HELLO, __TEST__O_TI_XTI_GET_STATE,
	__TEST__O_TI_XTI_CLEAR_EVENT, __TEST__O_TI_XTI_MODE,
	__TEST__O_TI_TLI_MODE,
	__TEST_TI_GETINFO, __TEST_TI_OPTMGMT, __TEST_TI_BIND,
	__TEST_TI_UNBIND, __TEST_TI_GETMYNAME, __TEST_TI_GETPEERNAME,
	__TEST_TI_SETMYNAME, __TEST_TI_SETPEERNAME, __TEST_TI_SYNC,
	__TEST_TI_GETADDRS, __TEST_TI_CAPABILITY,
	__TEST_TI_SETMYNAME_DATA, __TEST_TI_SETPEERNAME_DATA,
	__TEST_TI_SETMYNAME_DISC, __TEST_TI_SETPEERNAME_DISC,
	__TEST_TI_SETMYNAME_DISC_DATA, __TEST_TI_SETPEERNAME_DISC_DATA,
	__TEST_PRIM_TOO_SHORT, __TEST_PRIM_WAY_TOO_SHORT,
};

enum {
	__STATUS_OUT_OF_SERVICE = 200, __STATUS_ALIGNMENT, __STATUS_PROVING_NORMAL, __STATUS_PROVING_EMERG,
	__STATUS_IN_SERVICE, __STATUS_PROCESSOR_OUTAGE, __STATUS_PROCESSOR_ENDED, __STATUS_BUSY,
	__STATUS_BUSY_ENDED, __STATUS_INVALID_STATUS, __STATUS_SEQUENCE_SYNC, __MSG_PROVING,
	__TEST_ACK, __TEST_TX_BREAK, __TEST_TX_MAKE, __TEST_BAD_ACK, __TEST_MSU_TOO_SHORT,
	__TEST_FISU, __TEST_FISU_S, __TEST_FISU_CORRUPT, __TEST_FISU_CORRUPT_S,
	__TEST_MSU_SEVEN_ONES, __TEST_MSU_TOO_LONG, __TEST_FISU_FISU_1FLAG, __TEST_FISU_FISU_2FLAG,
	__TEST_MSU_MSU_1FLAG, __TEST_MSU_MSU_2FLAG, __TEST_COUNT, __TEST_TRIES,
	__TEST_ETC, __TEST_SIB_S,
	__TEST_FISU_BAD_FIB, __TEST_LSSU_CORRUPT, __TEST_LSSU_CORRUPT_S,
	__TEST_MSU, __TEST_MSU_S,
	__TEST_SIO, __TEST_SIN, __TEST_SIE, __TEST_SIOS, __TEST_SIPO, __TEST_SIB, __TEST_SIX,
	__TEST_SIO2, __TEST_SIN2, __TEST_SIE2, __TEST_SIOS2, __TEST_SIPO2, __TEST_SIB2, __TEST_SIX2,
};

#define __EVENT_IUT_IN_SERVICE	    __STATUS_IN_SERVICE
#define __EVENT_IUT_OUT_OF_SERVICE  __STATUS_OUT_OF_SERVICE
#define __EVENT_IUT_RPO		    __STATUS_PROCESSOR_OUTAGE
#define __EVENT_IUT_RPR		    __STATUS_PROCESSOR_ENDED
#define __EVENT_IUT_DATA	    __TEST_DATA

enum {
	__TEST_POWER_ON = 300, __TEST_START, __TEST_STOP, __TEST_LPO, __TEST_LPR, __TEST_EMERG,
	__TEST_CEASE, __TEST_SEND_MSU, __TEST_SEND_MSU_S, __TEST_CONG_A, __TEST_CONG_D,
	__TEST_NO_CONG, __TEST_CLEARB, __TEST_SYNC, __TEST_CONTINUE,
};

enum {
	__TEST_ATTACH_REQ = 400, __TEST_DETACH_REQ, __TEST_ENABLE_REQ, __TEST_ENABLE_CON,
	__TEST_DISABLE_REQ, __TEST_DISABLE_CON, __TEST_ERROR_IND, __TEST_SDL_OPTIONS,
	__TEST_SDL_CONFIG, __TEST_SDT_OPTIONS, __TEST_SDT_CONFIG, __TEST_SL_OPTIONS,
	__TEST_SL_CONFIG, __TEST_SDL_STATS, __TEST_SDT_STATS, __TEST_SL_STATS,
};

/*
 *  -------------------------------------------------------------------------
 *
 *  Configuration
 *
 *  -------------------------------------------------------------------------
 */

static int ss7_pvar = SS7_PVAR_ITUT_00;

struct test_stats {
	sdl_stats_t sdl;
	sdt_stats_t sdt;
	sl_stats_t sl;
} iutstat = {};

struct test_config {
	lmi_option_t opt;
	sdl_config_t sdl;
	sdt_config_t sdt;
	sl_config_t sl;
} iutconf = {
	{
		SS7_PVAR_ITUT_96,	/* pvar */
		    0,		/* popt */
	},			/* opt */
	{
		.ifname = NULL,	/* */
		    .ifflags = 0,	/* */
		    .iftype = SDL_TYPE_DS0,	/* */
		    .ifrate = 64000,	/* */
		    .ifgtype = SDL_GTYPE_NONE,	/* */
		    .ifgrate = 0,	/* */
		    .ifmode = SDL_MODE_NONE,	/* */
		    .ifgmode = SDL_GMODE_NONE,	/* */
		    .ifgcrc = SDL_GCRC_NONE,	/* */
		    .ifclock = SDL_CLOCK_NONE,	/* */
		    .ifcoding = SDL_CODING_NONE,	/* */
		    .ifframing = SDL_FRAMING_NONE,	/* */
		    .ifblksize = 0,	/* */
		    .ifleads = 0,	/* */
		    .ifbpv = 0,	/* */
		    .ifalarms = 0,	/* */
		    .ifrxlevel = 0,	/* */
		    .iftxlevel = 0,	/* */
		    .ifsync = 0,	/* */
	},			/* sdl */
	{
		.t8 = 100,	/* t8 - T8 timeout (milliseconds) */
		    .Tin = 4,	/* Tin - AERM normal proving threshold */
		    .Tie = 1,	/* Tie - AERM emergency proving threshold */
		    .T = 64,	/* T - SUERM error threshold */
		    .D = 256,	/* D - SUERM error rate parameter */
		    .Te = 577169,	/* Te - EIM error threshold */
		    .De = 9308000,	/* De - EIM correct decrement */
		    .Ue = 144292000,	/* Ue - EIM error increment */
		    .N = 16,	/* N */
		    .m = 272,	/* m */
		    .b = 64,	/* b */
		    .f = SDT_FLAGS_ONE,	/* f */
	},			/* sdt */
	{
		.t1 = 45 * 1000,	/* t1 - timer t1 duration (milliseconds) */
		    .t2 = 5 * 1000,	/* t2 - timer t2 duration (milliseconds) */
		    .t2l = 20 * 1000,	/* t2l - timer t2l duration (milliseconds) */
		    .t2h = 100 * 1000,	/* t2h - timer t2h duration (milliseconds) */
		    .t3 = 1 * 1000,	/* t3 - timer t3 duration (milliseconds) */
		    .t4n = 8 * 1000,	/* t4n - timer t4n duration (milliseconds) */
		    .t4e = 500,		/* t4e - timer t4e duration (milliseconds) */
		    .t5 = 125,		/* t5 - timer t5 duration (milliseconds) */
		    .t6 = 4 * 1000,	/* t6 - timer t6 duration (milliseconds) */
		    .t7 = 2 * 1000,	/* t7 - timer t7 duration (milliseconds) */
		    .rb_abate = 3,	/* rb_abate - RB cong abatement (#msgs) */
		    .rb_accept = 6,	/* rb_accept - RB cong onset accept (#msgs) */
		    .rb_discard = 9,	/* rb_discard - RB cong discard (#msgs) */
		    .tb_abate_1 = 128 * 272,	/* tb_abate_1 - lev 1 cong abate (#bytes) */
		    .tb_onset_1 = 256 * 272,	/* tb_onset_1 - lev 1 cong onset (#bytes) */
		    .tb_discd_1 = 384 * 272,	/* tb_discd_1 - lev 1 cong discard (#bytes) */
		    .tb_abate_2 = 512 * 272,	/* tb_abate_2 - lev 1 cong abate (#bytes) */
		    .tb_onset_2 = 640 * 272,	/* tb_onset_2 - lev 1 cong onset (#bytes) */
		    .tb_discd_2 = 768 * 272,	/* tb_discd_2 - lev 1 cong discard (#bytes) */
		    .tb_abate_3 = 896 * 272,	/* tb_abate_3 - lev 1 cong abate (#bytes) */
		    .tb_onset_3 = 1024 * 272,	/* tb_onset_3 - lev 1 cong onset (#bytes) */
		    .tb_discd_3 = 1152 * 272,	/* tb_discd_3 - lev 1 cong discard (#bytes) */
		    .N1 = 31,	/* N1 - PCR/RTBmax messages (#msg) */
		    .N2 = 8192,	/* N2 - PCR/RTBmax octets (#bytes) */
		    .M = 5	/* M - IAC normal proving periods */
} /* sl */ };

struct test_config *config = &iutconf;
struct test_stats  *stats = &iutstat;

/*
 *  -------------------------------------------------------------------------
 *
 *  Timer Functions
 *
 *  -------------------------------------------------------------------------
 */

/*
 *  Timer values for tests: each timer has a low range (minus error margin)
 *  and a high range (plus error margin).
 */

static long timer_scale = 1;

#define TEST_TIMEOUT 5000

typedef struct timer_range {
	long lo;
	long hi;
	char *name;
} timer_range_t;

enum { t1 = 0, t2, t3, t4n, t4e, t5, t6, t7, tmax };

/* *INDENT-OFF* */
static timer_range_t timer[tmax] = {
	{40000,			50000,		"T1"	},	/* Timer T1 30000 */
	{5000,			150000,		"T2"	},	/* Timer T2 5000 */
	{1000,			1500,		"T3"	},	/* Timer T3 100 */
	{7500,			9500,		"T4(Pn)"},	/* Timer T4n 3000 */
	{200,			800,		"T4(Pe)"},	/* Timer T4e 50 */
	{125,			125,		"T5"	},	/* Timer T5 10 */
	{3000,			6000,		"T6"	},	/* Timer T6 300 */
	{500,			2000,		"T7"	}	/* Timer T7 50 */
};
/* *INDENT-ON* */

long test_start = 0;

static int state = 0;
static const char *failure_string = NULL;

#define __stringify_1(x) #x
#define __stringify(x) __stringify_1(x)
#define FAILURE_STRING(string) "[" __stringify(__LINE__) "] " string

/* lockf does not work well on SMP for some reason */
#if 1
#undef lockf
#define lockf(x,y,z) 0
#endif

/*
 *  Return the current time in milliseconds.
 */
static long
dual_milliseconds(int child, int t1, int t2)
{
	long ret;
	struct timeval now;
	static const char *msgs[] = {
		"             %1$-6.6s !      %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld     :                    [%7$d:%8$03d]\n",
		"                    :      %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld     ! %1$-6.6s             [%7$d:%8$03d]\n",
		"                    :      %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld  !  : %1$-6.6s             [%7$d:%8$03d]\n",
		"                    !  %1$-6.6s %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld  !                    [%7$d:%8$03d]\n",
	};
	static const char *blank[] = {
		"                    !                                   :                    \n",
		"                    :                                   !                    \n",
		"                    :                                !  :                    \n",
		"                    !                                   !                    \n",
	};
	static const char *plus[] = {
		"               +    !                                   :                    \n",
		"                    :                                   !    +               \n",
		"                    :                                !  :    +               \n",
		"                    !      +                            !                    \n",
	};

	gettimeofday(&now, NULL);
	if (!test_start)	/* avoid blowing over precision */
		test_start = now.tv_sec;
	ret = (now.tv_sec - test_start) * 1000;
	ret += (now.tv_usec + 500) / 1000;

	if (show && verbose > 0) {
		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, blank[child]);
		fprintf(stdout, msgs[child], timer[t1].name, timer[t1].lo / 1000, timer[t1].lo - ((timer[t1].lo / 1000) * 1000), timer[t1].name, timer[t1].hi / 1000, timer[t1].hi - ((timer[t1].hi / 1000) * 1000), child, state);
		fprintf(stdout, plus[child]);
		fprintf(stdout, msgs[child], timer[t2].name, timer[t2].lo / 1000, timer[t2].lo - ((timer[t2].lo / 1000) * 1000), timer[t2].name, timer[t2].hi / 1000, timer[t2].hi - ((timer[t2].hi / 1000) * 1000), child, state);
		fprintf(stdout, blank[child]);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}

	return ret;
}

/*
 *  Return the current time in milliseconds.
 */
static long
milliseconds(int child, int t)
{
	long ret;
	struct timeval now;
	static const char *msgs[] = {
		"             %1$-6.6s !      %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld     :                    [%7$d:%8$03d]\n",
		"                    :      %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld     ! %1$-6.6s             [%7$d:%8$03d]\n",
		"                    :      %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld  !  : %1$-6.6s             [%7$d:%8$03d]\n",
		"                    !  %1$-6.6s %2$3ld.%3$03ld <= %4$-2.2s <= %5$3ld.%6$03ld  !                    [%7$d:%8$03d]\n",
	};
	static const char *blank[] = {
		"                    !                                   :                    \n",
		"                    :                                   !                    \n",
		"                    :                                !  :                    \n",
		"                    !                                   !                    \n",
	};

	gettimeofday(&now, NULL);
	if (!test_start)	/* avoid blowing over precision */
		test_start = now.tv_sec;
	ret = (now.tv_sec - test_start) * 1000;
	ret += (now.tv_usec + 500) / 1000;

	if (show && verbose > 0) {
		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, blank[child]);
		fprintf(stdout, msgs[child], timer[t].name, timer[t].lo / 1000, timer[t].lo - ((timer[t].lo / 1000) * 1000), timer[t].name, timer[t].hi / 1000, timer[t].hi - ((timer[t].hi / 1000) * 1000), child, state);
		fprintf(stdout, blank[child]);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}

	return ret;
}

/*
 *  Check the current time against the beginning time provided as an argnument
 *  and see if the time inverval falls between the low and high values for the
 *  timer as specified by arguments.  Return SUCCESS if the interval is within
 *  the allowable range and FAILURE otherwise.
 */
static int
check_time(int child, const char *t, long beg, long lo, long hi)
{
	long i;
	struct timeval now;
	static const char *msgs[] = {
		"       check %1$-6.6s ? [%2$3ld.%3$03ld <= %4$3ld.%5$03ld <= %6$3ld.%7$03ld]   |                    [%8$d:%9$03d]\n",
		"                    | [%2$3ld.%3$03ld <= %4$3ld.%5$03ld <= %6$3ld.%7$03ld]   ? %1$-6.6s check       [%8$d:%9$03d]\n",
		"                    | [%2$3ld.%3$03ld <= %4$3ld.%5$03ld <= %6$3ld.%7$03ld]?  | %1$-6.6s check       [%8$d:%9$03d]\n",
		"       check %1$-6.6s ? [%2$3ld.%3$03ld <= %4$3ld.%5$03ld <= %6$3ld.%7$03ld]   ?                    [%8$d:%9$03d]\n",
	};

	if (gettimeofday(&now, NULL)) {
		printf("****ERROR: gettimeofday\n");
		printf("           %s: %s\n", __FUNCTION__, strerror(errno));
		fflush(stdout);
		goto failure;
	}

	i = (now.tv_sec - test_start) * 1000;
	i += (now.tv_usec + 500) / 1000;
	i -= beg;

	if (show && verbose > 0) {
		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, msgs[child], t, (lo - 100) / 1000, (lo - 100) - (((lo - 100) / 1000) * 1000), i / 1000, i - ((i / 1000) * 1000), (hi + 100) / 1000, (hi + 100) - (((hi + 100) / 1000) * 1000), child, state);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}

	if (lo - 100 <= i && i <= hi + 100)
		return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
time_event(int child, int event)
{
	static const char *msgs[] = {
		"                    ! %11.6g                |                    <%d:%03d>\n",
		"                    |                %11.6g !                    <%d:%03d>\n",
		"                    |             %11.6g !  |                    <%d:%03d>\n",
		"                    !        %11.6g         !                    <%d:%03d>\n",
	};

	if ((verbose > 4 && show) || (verbose > 5 && show_msg)) {
		float t, m;
		struct timeval now;

		gettimeofday(&now, NULL);
		if (!test_start)
			test_start = now.tv_sec;
		t = (now.tv_sec - test_start);
		m = now.tv_usec;
		m = m / 1000000;
		t += m;

		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, msgs[child], t, child, state);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}
	return (event);
}

static int timer_timeout = 0;
static int last_signum = 0;

static void
signal_handler(int signum)
{
	last_signum = signum;
	if (signum == SIGALRM)
		timer_timeout = 1;
	return;
}

static int
start_signals(void)
{
	sigset_t mask;
	struct sigaction act;

	act.sa_handler = signal_handler;
//      act.sa_flags = SA_RESTART | SA_ONESHOT;
	act.sa_flags = 0;
//      act.sa_restorer = NULL;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGALRM, &act, NULL))
		goto failure;
	if (sigaction(SIGPOLL, &act, NULL))
		goto failure;
	if (sigaction(SIGURG, &act, NULL))
		goto failure;
	if (sigaction(SIGPIPE, &act, NULL))
		goto failure;
	if (sigaction(SIGHUP, &act, NULL))
		goto failure;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPOLL);
	sigaddset(&mask, SIGURG);
	sigaddset(&mask, SIGPIPE);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	siginterrupt(SIGALRM, 1);
	siginterrupt(SIGPOLL, 1);
	siginterrupt(SIGURG, 1);
	siginterrupt(SIGPIPE, 1);
	siginterrupt(SIGHUP, 1);
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

/*
 *  Start an interval timer as the overall test timer.
 */
static int
start_tt(long duration)
{
	struct itimerval setting = {
		{0, 0},
		{duration / 1000, (duration % 1000) * 1000}
	};

	if (duration == (long) INFINITE_WAIT)
		return __RESULT_SUCCESS;
	if (start_signals())
		goto failure;
	if (setitimer(ITIMER_REAL, &setting, NULL))
		goto failure;
	timer_timeout = 0;
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

#if 0
static int
start_st(long duration)
{
	long sdur = (duration + timer_scale - 1) / timer_scale;

	return start_tt(sdur);
}
#endif

static int
stop_signals(void)
{
	int result = __RESULT_SUCCESS;
	sigset_t mask;
	struct sigaction act;

	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;
//	act.sa_restorer = NULL;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGALRM, &act, NULL))
		result = __RESULT_FAILURE;
	if (sigaction(SIGPOLL, &act, NULL))
		result = __RESULT_FAILURE;
	if (sigaction(SIGURG, &act, NULL))
		result = __RESULT_FAILURE;
	if (sigaction(SIGPIPE, &act, NULL))
		result = __RESULT_FAILURE;
	if (sigaction(SIGHUP, &act, NULL))
		result = __RESULT_FAILURE;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPOLL);
	sigaddset(&mask, SIGURG);
	sigaddset(&mask, SIGPIPE);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	return (result);
}

static int
stop_tt(void)
{
	struct itimerval setting = { {0, 0}, {0, 0} };
	int result = __RESULT_SUCCESS;

	if (setitimer(ITIMER_REAL, &setting, NULL))
		return __RESULT_FAILURE;
	if (stop_signals() != __RESULT_SUCCESS)
		result = __RESULT_FAILURE;
	timer_timeout = 0;
	return (result);
}

static long beg_time = 0;

static int event = 0;
static int expand = 0;

static int oldmsg = 0;
static int cntmsg = 0;
static int oldisb = 0;
static int oldret = 0;
static int cntret = 0;

#define TEST_PORT_NUMBER 18000

/*
 *  -------------------------------------------------------------------------
 *
 *  Printing things
 *
 *  -------------------------------------------------------------------------
 */

char *
errno_string(long err)
{
	switch (err) {
	case 0:
		return ("ok");
	case EPERM:
		return ("[EPERM]");
	case ENOENT:
		return ("[ENOENT]");
	case ESRCH:
		return ("[ESRCH]");
	case EINTR:
		return ("[EINTR]");
	case EIO:
		return ("[EIO]");
	case ENXIO:
		return ("[ENXIO]");
	case E2BIG:
		return ("[E2BIG]");
	case ENOEXEC:
		return ("[ENOEXEC]");
	case EBADF:
		return ("[EBADF]");
	case ECHILD:
		return ("[ECHILD]");
	case EAGAIN:
		return ("[EAGAIN]");
	case ENOMEM:
		return ("[ENOMEM]");
	case EACCES:
		return ("[EACCES]");
	case EFAULT:
		return ("[EFAULT]");
	case ENOTBLK:
		return ("[ENOTBLK]");
	case EBUSY:
		return ("[EBUSY]");
	case EEXIST:
		return ("[EEXIST]");
	case EXDEV:
		return ("[EXDEV]");
	case ENODEV:
		return ("[ENODEV]");
	case ENOTDIR:
		return ("[ENOTDIR]");
	case EISDIR:
		return ("[EISDIR]");
	case EINVAL:
		return ("[EINVAL]");
	case ENFILE:
		return ("[ENFILE]");
	case EMFILE:
		return ("[EMFILE]");
	case ENOTTY:
		return ("[ENOTTY]");
	case ETXTBSY:
		return ("[ETXTBSY]");
	case EFBIG:
		return ("[EFBIG]");
	case ENOSPC:
		return ("[ENOSPC]");
	case ESPIPE:
		return ("[ESPIPE]");
	case EROFS:
		return ("[EROFS]");
	case EMLINK:
		return ("[EMLINK]");
	case EPIPE:
		return ("[EPIPE]");
	case EDOM:
		return ("[EDOM]");
	case ERANGE:
		return ("[ERANGE]");
	case EDEADLK:
		return ("[EDEADLK]");
	case ENAMETOOLONG:
		return ("[ENAMETOOLONG]");
	case ENOLCK:
		return ("[ENOLCK]");
	case ENOSYS:
		return ("[ENOSYS]");
	case ENOTEMPTY:
		return ("[ENOTEMPTY]");
	case ELOOP:
		return ("[ELOOP]");
	case ENOMSG:
		return ("[ENOMSG]");
	case EIDRM:
		return ("[EIDRM]");
	case ECHRNG:
		return ("[ECHRNG]");
	case EL2NSYNC:
		return ("[EL2NSYNC]");
	case EL3HLT:
		return ("[EL3HLT]");
	case EL3RST:
		return ("[EL3RST]");
	case ELNRNG:
		return ("[ELNRNG]");
	case EUNATCH:
		return ("[EUNATCH]");
	case ENOCSI:
		return ("[ENOCSI]");
	case EL2HLT:
		return ("[EL2HLT]");
	case EBADE:
		return ("[EBADE]");
	case EBADR:
		return ("[EBADR]");
	case EXFULL:
		return ("[EXFULL]");
	case ENOANO:
		return ("[ENOANO]");
	case EBADRQC:
		return ("[EBADRQC]");
	case EBADSLT:
		return ("[EBADSLT]");
	case EBFONT:
		return ("[EBFONT]");
	case ENOSTR:
		return ("[ENOSTR]");
	case ENODATA:
		return ("[ENODATA]");
	case ETIME:
		return ("[ETIME]");
	case ENOSR:
		return ("[ENOSR]");
	case ENONET:
		return ("[ENONET]");
	case ENOPKG:
		return ("[ENOPKG]");
	case EREMOTE:
		return ("[EREMOTE]");
	case ENOLINK:
		return ("[ENOLINK]");
	case EADV:
		return ("[EADV]");
	case ESRMNT:
		return ("[ESRMNT]");
	case ECOMM:
		return ("[ECOMM]");
	case EPROTO:
		return ("[EPROTO]");
	case EMULTIHOP:
		return ("[EMULTIHOP]");
	case EDOTDOT:
		return ("[EDOTDOT]");
	case EBADMSG:
		return ("[EBADMSG]");
	case EOVERFLOW:
		return ("[EOVERFLOW]");
	case ENOTUNIQ:
		return ("[ENOTUNIQ]");
	case EBADFD:
		return ("[EBADFD]");
	case EREMCHG:
		return ("[EREMCHG]");
	case ELIBACC:
		return ("[ELIBACC]");
	case ELIBBAD:
		return ("[ELIBBAD]");
	case ELIBSCN:
		return ("[ELIBSCN]");
	case ELIBMAX:
		return ("[ELIBMAX]");
	case ELIBEXEC:
		return ("[ELIBEXEC]");
	case EILSEQ:
		return ("[EILSEQ]");
	case ERESTART:
		return ("[ERESTART]");
	case ESTRPIPE:
		return ("[ESTRPIPE]");
	case EUSERS:
		return ("[EUSERS]");
	case ENOTSOCK:
		return ("[ENOTSOCK]");
	case EDESTADDRREQ:
		return ("[EDESTADDRREQ]");
	case EMSGSIZE:
		return ("[EMSGSIZE]");
	case EPROTOTYPE:
		return ("[EPROTOTYPE]");
	case ENOPROTOOPT:
		return ("[ENOPROTOOPT]");
	case EPROTONOSUPPORT:
		return ("[EPROTONOSUPPORT]");
	case ESOCKTNOSUPPORT:
		return ("[ESOCKTNOSUPPORT]");
	case EOPNOTSUPP:
		return ("[EOPNOTSUPP]");
	case EPFNOSUPPORT:
		return ("[EPFNOSUPPORT]");
	case EAFNOSUPPORT:
		return ("[EAFNOSUPPORT]");
	case EADDRINUSE:
		return ("[EADDRINUSE]");
	case EADDRNOTAVAIL:
		return ("[EADDRNOTAVAIL]");
	case ENETDOWN:
		return ("[ENETDOWN]");
	case ENETUNREACH:
		return ("[ENETUNREACH]");
	case ENETRESET:
		return ("[ENETRESET]");
	case ECONNABORTED:
		return ("[ECONNABORTED]");
	case ECONNRESET:
		return ("[ECONNRESET]");
	case ENOBUFS:
		return ("[ENOBUFS]");
	case EISCONN:
		return ("[EISCONN]");
	case ENOTCONN:
		return ("[ENOTCONN]");
	case ESHUTDOWN:
		return ("[ESHUTDOWN]");
	case ETOOMANYREFS:
		return ("[ETOOMANYREFS]");
	case ETIMEDOUT:
		return ("[ETIMEDOUT]");
	case ECONNREFUSED:
		return ("[ECONNREFUSED]");
	case EHOSTDOWN:
		return ("[EHOSTDOWN]");
	case EHOSTUNREACH:
		return ("[EHOSTUNREACH]");
	case EALREADY:
		return ("[EALREADY]");
	case EINPROGRESS:
		return ("[EINPROGRESS]");
	case ESTALE:
		return ("[ESTALE]");
	case EUCLEAN:
		return ("[EUCLEAN]");
	case ENOTNAM:
		return ("[ENOTNAM]");
	case ENAVAIL:
		return ("[ENAVAIL]");
	case EISNAM:
		return ("[EISNAM]");
	case EREMOTEIO:
		return ("[EREMOTEIO]");
	case EDQUOT:
		return ("[EDQUOT]");
	case ENOMEDIUM:
		return ("[ENOMEDIUM]");
	case EMEDIUMTYPE:
		return ("[EMEDIUMTYPE]");
	default:
	{
		static char buf[32];

		snprintf(buf, sizeof(buf), "[%ld]", (long) err);
		return buf;
	}
	}
}

const char *
lmi_strreason(unsigned int reason)
{
	switch (reason) {
	default:
	case LMI_UNSPEC:
		return ("Unknown or unspecified");
	case LMI_BADADDRESS:
		return ("Address was invalid");
	case LMI_BADADDRTYPE:
		return ("Invalid address type");
	case LMI_BADDIAL:
		return ("(not used)");
	case LMI_BADDIALTYPE:
		return ("(not used)");
	case LMI_BADDISPOSAL:
		return ("Invalid disposal parameter");
	case LMI_BADFRAME:
		return ("Defective SDU received");
	case LMI_BADPPA:
		return ("Invalid PPA identifier");
	case LMI_BADPRIM:
		return ("Unregognized primitive");
	case LMI_DISC:
		return ("Disconnected");
	case LMI_EVENT:
		return ("Protocol-specific event ocurred");
	case LMI_FATALERR:
		return ("Device has become unusable");
	case LMI_INITFAILED:
		return ("Link initialization failed");
	case LMI_NOTSUPP:
		return ("Primitive not supported by this device");
	case LMI_OUTSTATE:
		return ("Primitive was issued from invalid state");
	case LMI_PROTOSHORT:
		return ("M_PROTO block too short");
	case LMI_SYSERR:
		return ("UNIX system error");
	case LMI_WRITEFAIL:
		return ("Unitdata request failed");
	case LMI_CRCERR:
		return ("CRC or FCS error");
	case LMI_DLE_EOT:
		return ("DLE EOT detected");
	case LMI_FORMAT:
		return ("Format error detected");
	case LMI_HDLC_ABORT:
		return ("Aborted frame detected");
	case LMI_OVERRUN:
		return ("Input overrun");
	case LMI_TOOSHORT:
		return ("Frame too short");
	case LMI_INCOMPLETE:
		return ("Partial frame received");
	case LMI_BUSY:
		return ("Telephone was busy");
	case LMI_NOANSWER:
		return ("Connection went unanswered");
	case LMI_CALLREJECT:
		return ("Connection rejected");
	case LMI_HDLC_IDLE:
		return ("HDLC line went idle");
	case LMI_HDLC_NOTIDLE:
		return ("HDLC link no longer idle");
	case LMI_QUIESCENT:
		return ("Line being reassigned");
	case LMI_RESUMED:
		return ("Line has been reassigned");
	case LMI_DSRTIMEOUT:
		return ("Did not see DSR in time");
	case LMI_LAN_COLLISIONS:
		return ("LAN excessive collisions");
	case LMI_LAN_REFUSED:
		return ("LAN message refused");
	case LMI_LAN_NOSTATION:
		return ("LAN no such station");
	case LMI_LOSTCTS:
		return ("Lost Clear to Send signal");
	case LMI_DEVERR:
		return ("Start of device-specific error codes");
	}
}

char *
lmerrno_string(long uerr, ulong lmerr)
{
	switch (lmerr) {
	case LMI_UNSPEC:	/* Unknown or unspecified */
		return ("[LMI_UNSPEC]");
	case LMI_BADADDRESS:	/* Address was invalid */
		return ("[LMI_BADADDRESS]");
	case LMI_BADADDRTYPE:	/* Invalid address type */
		return ("[LMI_BADADDRTYPE]");
	case LMI_BADDIAL:	/* (not used) */
		return ("[LMI_BADDIAL]");
	case LMI_BADDIALTYPE:	/* (not used) */
		return ("[LMI_BADDIALTYPE]");
	case LMI_BADDISPOSAL:	/* Invalid disposal parameter */
		return ("[LMI_BADDISPOSAL]");
	case LMI_BADFRAME:	/* Defective SDU received */
		return ("[LMI_BADFRAME]");
	case LMI_BADPPA:	/* Invalid PPA identifier */
		return ("[LMI_BADPPA]");
	case LMI_BADPRIM:	/* Unregognized primitive */
		return ("[LMI_BADPRIM]");
	case LMI_DISC:		/* Disconnected */
		return ("[LMI_DISC]");
	case LMI_EVENT:	/* Protocol-specific event ocurred */
		return ("[LMI_EVENT]");
	case LMI_FATALERR:	/* Device has become unusable */
		return ("[LMI_FATALERR]");
	case LMI_INITFAILED:	/* Link initialization failed */
		return ("[LMI_INITFAILED]");
	case LMI_NOTSUPP:	/* Primitive not supported by this device */
		return ("[LMI_NOTSUPP]");
	case LMI_OUTSTATE:	/* Primitive was issued from invalid state */
		return ("[LMI_OUTSTATE]");
	case LMI_PROTOSHORT:	/* M_PROTO block too short */
		return ("[LMI_PROTOSHORT]");
	case LMI_SYSERR:	/* UNIX system error */
		return errno_string(uerr);
	case LMI_WRITEFAIL:	/* Unitdata request failed */
		return ("[LMI_WRITEFAIL]");
	case LMI_CRCERR:	/* CRC or FCS error */
		return ("[LMI_CRCERR]");
	case LMI_DLE_EOT:	/* DLE EOT detected */
		return ("[LMI_DLE_EOT]");
	case LMI_FORMAT:	/* Format error detected */
		return ("[LMI_FORMAT]");
	case LMI_HDLC_ABORT:	/* Aborted frame detected */
		return ("[LMI_HDLC_ABORT]");
	case LMI_OVERRUN:	/* Input overrun */
		return ("[LMI_OVERRUN]");
	case LMI_TOOSHORT:	/* Frame too short */
		return ("[LMI_TOOSHORT]");
	case LMI_INCOMPLETE:	/* Partial frame received */
		return ("[LMI_INCOMPLETE]");
	case LMI_BUSY:		/* Telephone was busy */
		return ("[LMI_BUSY]");
	case LMI_NOANSWER:	/* Connection went unanswered */
		return ("[LMI_NOANSWER]");
	case LMI_CALLREJECT:	/* Connection rejected */
		return ("[LMI_CALLREJECT]");
	case LMI_HDLC_IDLE:	/* HDLC line went idle */
		return ("[LMI_HDLC_IDLE]");
	case LMI_HDLC_NOTIDLE:	/* HDLC link no longer idle */
		return ("[LMI_HDLC_NOTIDLE]");
	case LMI_QUIESCENT:	/* Line being reassigned */
		return ("[LMI_QUIESCENT]");
	case LMI_RESUMED:	/* Line has been reassigned */
		return ("[LMI_RESUMED]");
	case LMI_DSRTIMEOUT:	/* Did not see DSR in time */
		return ("[LMI_DSRTIMEOUT]");
	case LMI_LAN_COLLISIONS:	/* LAN excessive collisions */
		return ("[LMI_LAN_COLLISIONS]");
	case LMI_LAN_REFUSED:	/* LAN message refused */
		return ("[LMI_LAN_REFUSED]");
	case LMI_LAN_NOSTATION:	/* LAN no such station */
		return ("[LMI_LAN_NOSTATION]");
	case LMI_LOSTCTS:	/* Lost Clear to Send signal */
		return ("[LMI_LOSTCTS]");
	case LMI_DEVERR:	/* Start of device-specific error codes */
		return ("[LMI_DEVERR]");
	default:
	{
		static char buf[32];

		snprintf(buf, sizeof(buf), "[%lu]", (ulong) lmerr);
		return buf;
	}
	}
}

const char *
event_string(int child, int event)
{
	switch (child) {
	case CHILD_IUT:
		switch (event) {
		case __EVENT_IUT_OUT_OF_SERVICE:
			return ("!out of service");
		case __EVENT_IUT_IN_SERVICE:
			return ("!in service");
		case __EVENT_IUT_RPO:
			return ("!rpo");
		case __EVENT_IUT_RPR:
			return ("!rpr");
		case __EVENT_IUT_DATA:
			return ("!msu");
		}
		break;
	default:
	case CHILD_PTU:
		switch (event) {
		case __STATUS_OUT_OF_SERVICE:
			return ("OUT-OF-SERVICE");
		case __STATUS_ALIGNMENT:
			return ("ALIGNMENT");
		case __STATUS_PROVING_NORMAL:
			return ("PROVING-NORMAL");
		case __STATUS_PROVING_EMERG:
			return ("PROVING-EMERGENCY");
		case __STATUS_IN_SERVICE:
			return ("IN-SERVICE");
		case __STATUS_PROCESSOR_OUTAGE:
			return ("PROCESSOR-OUTAGE");
		case __STATUS_PROCESSOR_ENDED:
			return ("PROCESSOR-ENDED");
		case __STATUS_BUSY:
			return ("BUSY");
		case __STATUS_BUSY_ENDED:
			return ("BUSY-ENDED");
		case __STATUS_SEQUENCE_SYNC:
			return ("READY");
		case __MSG_PROVING:
			return ("PROVING");
		case __TEST_ACK:
			return ("ACK");
		case __TEST_DATA:
			return ("DATA");
		case __TEST_SIO:
			return ("SIO");
		case __TEST_SIN:
			return ("SIN");
		case __TEST_SIE:
			return ("SIE");
		case __TEST_SIOS:
			return ("SIOS");
		case __TEST_SIPO:
			return ("SIPO");
		case __TEST_SIB:
			return ("SIB");
		case __TEST_SIX:
			return ("SIX");
		case __TEST_SIO2:
			return ("SIO2");
		case __TEST_SIN2:
			return ("SIN2");
		case __TEST_SIE2:
			return ("SIE2");
		case __TEST_SIOS2:
			return ("SIOS2");
		case __TEST_SIPO2:
			return ("SIPO2");
		case __TEST_SIB2:
			return ("SIB2");
		case __TEST_SIX2:
			return ("SIX2");
		case __TEST_FISU:
			return ("FISU");
		case __TEST_FISU_S:
			return ("FISU_S");
		case __TEST_FISU_BAD_FIB:
			return ("FISU_BAD_FIB");
		case __TEST_FISU_CORRUPT:
			return ("FISU_CORRUPT");
		case __TEST_FISU_CORRUPT_S:
			return ("FISU_CORRUPT_S");
		case __TEST_LSSU_CORRUPT:
			return ("LSSU_CORRUPT");
		case __TEST_LSSU_CORRUPT_S:
			return ("LSSU_CORRUPT_S");
		case __TEST_MSU:
			return ("MSU");
		case __TEST_MSU_SEVEN_ONES:
			return ("MSU_SEVEN_ONES");
		case __TEST_MSU_TOO_LONG:
			return ("MSU_TOO_LONG");
		case __TEST_MSU_TOO_SHORT:
			return ("MSU_TOO_SHORT");
		case __TEST_TX_BREAK:
			return ("TX_BREAK");
		case __TEST_TX_MAKE:
			return ("TX_MAKE");
		case __TEST_FISU_FISU_1FLAG:
			return ("FISU_FISU_1FLAG");
		case __TEST_FISU_FISU_2FLAG:
			return ("FISU_FISU_2FLAG");
		case __TEST_MSU_MSU_1FLAG:
			return ("MSU_MSU_1FLAG");
		case __TEST_MSU_MSU_2FLAG:
			return ("MSU_MSU_2FLAG");
		}
		break;
	}
	switch (event) {
	case __EVENT_EOF:
		return ("END OF FILE");
	case __EVENT_NO_MSG:
		return ("NO MESSAGE");
	case __EVENT_TIMEOUT:
		return ("TIMEOUT");
	case __EVENT_UNKNOWN:
		return ("UNKNOWN");
	case __RESULT_DECODE_ERROR:
		return ("DECODE ERROR");
	case __RESULT_SCRIPT_ERROR:
		return ("SCRIPT ERROR");
	case __RESULT_INCONCLUSIVE:
		return ("INCONCLUSIVE");
	case __RESULT_SUCCESS:
		return ("SUCCESS");
	case __RESULT_FAILURE:
		return ("FAILURE");
	case __TEST_ENABLE_CON:
		return ("!enable con");
	case __TEST_DISABLE_CON:
		return ("!disable con");
	case __TEST_OK_ACK:
		return ("!ok ack");
	case __TEST_ERROR_ACK:
		return ("!error ack");
	case __TEST_ERROR_IND:
		return ("!error ind");
	case __TEST_COUNT:
		return ("COUNT");
	case __TEST_TRIES:
		return ("TRIES");
	case __TEST_ETC:
		return ("ETC");
	case __TEST_SIB_S:
		return ("SIB_S");
	case __TEST_POWER_ON:
		return ("POWER_ON");
	case __TEST_START:
		return ("START");
	case __TEST_STOP:
		return ("STOP");
	case __TEST_LPO:
		return ("LPO");
	case __TEST_LPR:
		return ("LPR");
	case __TEST_CONTINUE:
		return ("CONTINUE");
	case __TEST_EMERG:
		return ("EMERG");
	case __TEST_CEASE:
		return ("CEASE");
	case __TEST_SEND_MSU:
		return ("SEND_MSU");
	case __TEST_SEND_MSU_S:
		return ("SEND_MSU_S");
	case __TEST_CONG_A:
		return ("CONG_A");
	case __TEST_CONG_D:
		return ("CONG_D");
	case __TEST_NO_CONG:
		return ("NO_CONG");
	case __TEST_CLEARB:
		return ("CLEARB");
	}
	return ("(unexpected)");
}

const char *
ioctl_string(int cmd, intptr_t arg)
{
	switch (cmd) {
	case I_NREAD:
		return ("I_NREAD");
	case I_PUSH:
		return ("I_PUSH");
	case I_POP:
		return ("I_POP");
	case I_LOOK:
		return ("I_LOOK");
	case I_FLUSH:
		return ("I_FLUSH");
	case I_SRDOPT:
		return ("I_SRDOPT");
	case I_GRDOPT:
		return ("I_GRDOPT");
	case I_STR:
		if (arg) {
			struct strioctl *icp = (struct strioctl *) arg;

			switch (icp->ic_cmd) {
#if 0
			case _O_TI_BIND:
				return ("_O_TI_BIND");
			case O_TI_BIND:
				return ("O_TI_BIND");
			case _O_TI_GETINFO:
				return ("_O_TI_GETINFO");
			case O_TI_GETINFO:
				return ("O_TI_GETINFO");
			case _O_TI_GETMYNAME:
				return ("_O_TI_GETMYNAME");
			case _O_TI_GETPEERNAME:
				return ("_O_TI_GETPEERNAME");
			case _O_TI_OPTMGMT:
				return ("_O_TI_OPTMGMT");
			case O_TI_OPTMGMT:
				return ("O_TI_OPTMGMT");
			case _O_TI_TLI_MODE:
				return ("_O_TI_TLI_MODE");
			case _O_TI_UNBIND:
				return ("_O_TI_UNBIND");
			case O_TI_UNBIND:
				return ("O_TI_UNBIND");
			case _O_TI_XTI_CLEAR_EVENT:
				return ("_O_TI_XTI_CLEAR_EVENT");
			case _O_TI_XTI_GET_STATE:
				return ("_O_TI_XTI_GET_STATE");
			case _O_TI_XTI_HELLO:
				return ("_O_TI_XTI_HELLO");
			case _O_TI_XTI_MODE:
				return ("_O_TI_XTI_MODE");
			case TI_BIND:
				return ("TI_BIND");
			case TI_CAPABILITY:
				return ("TI_CAPABILITY");
			case TI_GETADDRS:
				return ("TI_GETADDRS");
			case TI_GETINFO:
				return ("TI_GETINFO");
			case TI_GETMYNAME:
				return ("TI_GETMYNAME");
			case TI_GETPEERNAME:
				return ("TI_GETPEERNAME");
			case TI_OPTMGMT:
				return ("TI_OPTMGMT");
			case TI_SETMYNAME:
				return ("TI_SETMYNAME");
			case TI_SETPEERNAME:
				return ("TI_SETPEERNAME");
			case TI_SYNC:
				return ("TI_SYNC");
			case TI_UNBIND:
				return ("TI_UNBIND");
#endif
			}
		}
		return ("I_STR");
	case I_SETSIG:
		return ("I_SETSIG");
	case I_GETSIG:
		return ("I_GETSIG");
	case I_FIND:
		return ("I_FIND");
	case I_LINK:
		return ("I_LINK");
	case I_UNLINK:
		return ("I_UNLINK");
	case I_RECVFD:
		return ("I_RECVFD");
	case I_PEEK:
		return ("I_PEEK");
	case I_FDINSERT:
		return ("I_FDINSERT");
	case I_SENDFD:
		return ("I_SENDFD");
#if 0
	case I_E_RECVFD:
		return ("I_E_RECVFD");
#endif
	case I_SWROPT:
		return ("I_SWROPT");
	case I_GWROPT:
		return ("I_GWROPT");
	case I_LIST:
		return ("I_LIST");
	case I_PLINK:
		return ("I_PLINK");
	case I_PUNLINK:
		return ("I_PUNLINK");
	case I_FLUSHBAND:
		return ("I_FLUSHBAND");
	case I_CKBAND:
		return ("I_CKBAND");
	case I_GETBAND:
		return ("I_GETBAND");
	case I_ATMARK:
		return ("I_ATMARK");
	case I_SETCLTIME:
		return ("I_SETCLTIME");
	case I_GETCLTIME:
		return ("I_GETCLTIME");
	case I_CANPUT:
		return ("I_CANPUT");
#if 0
	case I_SERROPT:
		return ("I_SERROPT");
	case I_GERROPT:
		return ("I_GERROPT");
	case I_ANCHOR:
		return ("I_ANCHOR");
#endif
#if 0
	case I_S_RECVFD:
		return ("I_S_RECVFD");
	case I_STATS:
		return ("I_STATS");
	case I_BIGPIPE:
		return ("I_BIGPIPE");
#endif
#if 0
	case I_GETTP:
		return ("I_GETTP");
	case I_AUTOPUSH:
		return ("I_AUTOPUSH");
	case I_HEAP_REPORT:
		return ("I_HEAP_REPORT");
	case I_FIFO:
		return ("I_FIFO");
	case I_GETMSG:
		return ("I_GETMSG");
	case I_PUTMSG:
		return ("I_PUTMSG");
	case I_PUTPMSG:
		return ("I_PUTPMSG");
	case I_GETPMSG:
		return ("I_GETPMSG");
	case I_FATTACH:
		return ("I_FATTACH");
	case I_FDETACH:
		return ("I_FDETACH");
	case I_PIPE:
		return ("I_PIPE");
#endif
	default:
		return ("(unexpected)");
	}
}

const char *
signal_string(int signum)
{
	switch (signum) {
	case SIGHUP:
		return ("SIGHUP");
	case SIGINT:
		return ("SIGINT");
	case SIGQUIT:
		return ("SIGQUIT");
	case SIGILL:
		return ("SIGILL");
	case SIGABRT:
		return ("SIGABRT");
	case SIGFPE:
		return ("SIGFPE");
	case SIGKILL:
		return ("SIGKILL");
	case SIGSEGV:
		return ("SIGSEGV");
	case SIGPIPE:
		return ("SIGPIPE");
	case SIGALRM:
		return ("SIGALRM");
	case SIGTERM:
		return ("SIGTERM");
	case SIGUSR1:
		return ("SIGUSR1");
	case SIGUSR2:
		return ("SIGUSR2");
	case SIGCHLD:
		return ("SIGCHLD");
	case SIGCONT:
		return ("SIGCONT");
	case SIGSTOP:
		return ("SIGSTOP");
	case SIGTSTP:
		return ("SIGTSTP");
	case SIGTTIN:
		return ("SIGTTIN");
	case SIGTTOU:
		return ("SIGTTOU");
	case SIGBUS:
		return ("SIGBUS");
	case SIGPOLL:
		return ("SIGPOLL");
	case SIGPROF:
		return ("SIGPROF");
	case SIGSYS:
		return ("SIGSYS");
	case SIGTRAP:
		return ("SIGTRAP");
	case SIGURG:
		return ("SIGURG");
	case SIGVTALRM:
		return ("SIGVTALRM");
	case SIGXCPU:
		return ("SIGXCPU");
	case SIGXFSZ:
		return ("SIGXFSZ");
	default:
		return ("unknown");
	}
}

const char *
poll_string(short events)
{
	if (events & POLLIN)
		return ("POLLIN");
	if (events & POLLPRI)
		return ("POLLPRI");
	if (events & POLLOUT)
		return ("POLLOUT");
	if (events & POLLRDNORM)
		return ("POLLRDNORM");
	if (events & POLLRDBAND)
		return ("POLLRDBAND");
	if (events & POLLWRNORM)
		return ("POLLWRNORM");
	if (events & POLLWRBAND)
		return ("POLLWRBAND");
	if (events & POLLERR)
		return ("POLLERR");
	if (events & POLLHUP)
		return ("POLLHUP");
	if (events & POLLNVAL)
		return ("POLLNVAL");
	if (events & POLLMSG)
		return ("POLLMSG");
	return ("none");
}

const char *
poll_events_string(short events)
{
	static char string[256] = "";
	int offset = 0, size = 256, len = 0;

	if (events & POLLIN) {
		len = snprintf(string + offset, size, "POLLIN|");
		offset += len;
		size -= len;
	}
	if (events & POLLPRI) {
		len = snprintf(string + offset, size, "POLLPRI|");
		offset += len;
		size -= len;
	}
	if (events & POLLOUT) {
		len = snprintf(string + offset, size, "POLLOUT|");
		offset += len;
		size -= len;
	}
	if (events & POLLRDNORM) {
		len = snprintf(string + offset, size, "POLLRDNORM|");
		offset += len;
		size -= len;
	}
	if (events & POLLRDBAND) {
		len = snprintf(string + offset, size, "POLLRDBAND|");
		offset += len;
		size -= len;
	}
	if (events & POLLWRNORM) {
		len = snprintf(string + offset, size, "POLLWRNORM|");
		offset += len;
		size -= len;
	}
	if (events & POLLWRBAND) {
		len = snprintf(string + offset, size, "POLLWRBAND|");
		offset += len;
		size -= len;
	}
	if (events & POLLERR) {
		len = snprintf(string + offset, size, "POLLERR|");
		offset += len;
		size -= len;
	}
	if (events & POLLHUP) {
		len = snprintf(string + offset, size, "POLLHUP|");
		offset += len;
		size -= len;
	}
	if (events & POLLNVAL) {
		len = snprintf(string + offset, size, "POLLNVAL|");
		offset += len;
		size -= len;
	}
	if (events & POLLMSG) {
		len = snprintf(string + offset, size, "POLLMSG|");
		offset += len;
		size -= len;
	}
	return (string);
}

void print_string(int child, const char *string);
void
print_ppa(int child, ppa_t * ppa)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%d:%d:%d", ((*ppa) >> 12) & 0x0f, ((*ppa) >> 8) & 0x0f, (*ppa) & 0x0ff);
	print_string(child, buf);
}
void print_string_val(int child, const char *string, ulong val);
void
print_sdl_stats(int child, sdl_stats_t * s)
{
	if (s->rx_octets)
		print_string_val(child, "rx_octets", s->rx_octets);
	if (s->tx_octets)
		print_string_val(child, "tx_octets", s->tx_octets);
	if (s->rx_overruns)
		print_string_val(child, "rx_overruns", s->rx_overruns);
	if (s->tx_underruns)
		print_string_val(child, "tx_underruns", s->tx_underruns);
	if (s->rx_buffer_overflows)
		print_string_val(child, "rx_buffer_overflows", s->rx_buffer_overflows);
	if (s->tx_buffer_overflows)
		print_string_val(child, "tx_buffer_overflows", s->tx_buffer_overflows);
	if (s->lead_cts_lost)
		print_string_val(child, "lead_cts_lost", s->lead_cts_lost);
	if (s->lead_dcd_lost)
		print_string_val(child, "lead_dcd_lost", s->lead_dcd_lost);
	if (s->carrier_lost)
		print_string_val(child, "carrier_lost", s->carrier_lost);
}

void
print_sdt_stats(int child, sdt_stats_t * s)
{
	if (s->tx_bytes)
		print_string_val(child, "tx_bytes", s->tx_bytes);
	if (s->tx_sus)
		print_string_val(child, "tx_sus", s->tx_sus);
	if (s->tx_sus_repeated)
		print_string_val(child, "tx_sus_repeated", s->tx_sus_repeated);
	if (s->tx_underruns)
		print_string_val(child, "tx_underruns", s->tx_underruns);
	if (s->tx_aborts)
		print_string_val(child, "tx_aborts", s->tx_aborts);
	if (s->tx_buffer_overflows)
		print_string_val(child, "tx_buffer_overflows", s->tx_buffer_overflows);
	if (s->tx_sus_in_error)
		print_string_val(child, "tx_sus_in_error", s->tx_sus_in_error);
	if (s->rx_bytes)
		print_string_val(child, "rx_bytes", s->rx_bytes);
	if (s->rx_sus)
		print_string_val(child, "rx_sus", s->rx_sus);
	if (s->rx_sus_compressed)
		print_string_val(child, "rx_sus_compressed", s->rx_sus_compressed);
	if (s->rx_overruns)
		print_string_val(child, "rx_overruns", s->rx_overruns);
	if (s->rx_aborts)
		print_string_val(child, "rx_aborts", s->rx_aborts);
	if (s->rx_buffer_overflows)
		print_string_val(child, "rx_buffer_overflows", s->rx_buffer_overflows);
	if (s->rx_sus_in_error)
		print_string_val(child, "rx_sus_in_error", s->rx_sus_in_error);
	if (s->rx_sync_transitions)
		print_string_val(child, "rx_sync_transitions", s->rx_sync_transitions);
	if (s->rx_bits_octet_counted)
		print_string_val(child, "rx_bits_octet_counted", s->rx_bits_octet_counted);
	if (s->rx_crc_errors)
		print_string_val(child, "rx_crc_errors", s->rx_crc_errors);
	if (s->rx_frame_errors)
		print_string_val(child, "rx_frame_errors", s->rx_frame_errors);
	if (s->rx_frame_overflows)
		print_string_val(child, "rx_frame_overflows", s->rx_frame_overflows);
	if (s->rx_frame_too_long)
		print_string_val(child, "rx_frame_too_long", s->rx_frame_too_long);
	if (s->rx_frame_too_short)
		print_string_val(child, "rx_frame_too_short", s->rx_frame_too_short);
	if (s->rx_residue_errors)
		print_string_val(child, "rx_residue_errors", s->rx_residue_errors);
	if (s->rx_length_error)
		print_string_val(child, "rx_length_error", s->rx_length_error);
	if (s->carrier_cts_lost)
		print_string_val(child, "carrier_cts_lost", s->carrier_cts_lost);
	if (s->carrier_dcd_lost)
		print_string_val(child, "carrier_dcd_lost", s->carrier_dcd_lost);
	if (s->carrier_lost)
		print_string_val(child, "carrier_lost", s->carrier_lost);
}
void
print_sl_stats(int child, sl_stats_t *s)
{
	if (s->sl_dur_in_service)
		print_string_val(child, "sl_dur_in_service", s->sl_dur_in_service);
	if (s->sl_fail_align_or_proving)
		print_string_val(child, "sl_fail_align_or_proving", s->sl_fail_align_or_proving);
	if (s->sl_nacks_received)
		print_string_val(child, "sl_nacks_received", s->sl_nacks_received);
	if (s->sl_dur_unavail)
		print_string_val(child, "sl_dur_unavail", s->sl_dur_unavail);
	if (s->sl_dur_unavail_failed)
		print_string_val(child, "sl_dur_unavail_failed", s->sl_dur_unavail_failed);
	if (s->sl_sibs_sent)
		print_string_val(child, "sl_sibs_sent", s->sl_sibs_sent);
	if (s->sl_tran_sio_sif_octets)
		print_string_val(child, "sl_tran_sio_sif_octets", s->sl_tran_sio_sif_octets);
	if (s->sl_tran_msus)
		print_string_val(child, "sl_tran_msus", s->sl_tran_msus);
	if (s->sl_recv_sio_sif_octets)
		print_string_val(child, "sl_recv_sio_sif_octets", s->sl_recv_sio_sif_octets);
	if (s->sl_recv_msus)
		print_string_val(child, "sl_recv_msus", s->sl_recv_msus);
	if (s->sl_cong_onset_ind[0])
		print_string_val(child, "sl_cong_onset_ind[0]", s->sl_cong_onset_ind[0]);
	if (s->sl_cong_onset_ind[1])
		print_string_val(child, "sl_cong_onset_ind[1]", s->sl_cong_onset_ind[1]);
	if (s->sl_cong_onset_ind[2])
		print_string_val(child, "sl_cong_onset_ind[2]", s->sl_cong_onset_ind[2]);
	if (s->sl_cong_onset_ind[3])
		print_string_val(child, "sl_cong_onset_ind[3]", s->sl_cong_onset_ind[3]);
	if (s->sl_dur_cong_status[0])
		print_string_val(child, "sl_dur_cong_status[0]", s->sl_dur_cong_status[0]);
	if (s->sl_dur_cong_status[1])
		print_string_val(child, "sl_dur_cong_status[1]", s->sl_dur_cong_status[1]);
	if (s->sl_dur_cong_status[2])
		print_string_val(child, "sl_dur_cong_status[2]", s->sl_dur_cong_status[2]);
	if (s->sl_dur_cong_status[3])
		print_string_val(child, "sl_dur_cong_status[3]", s->sl_dur_cong_status[3]);
	if (s->sl_cong_discd_ind[0])
		print_string_val(child, "sl_cong_discd_ind[0]", s->sl_cong_discd_ind[0]);
	if (s->sl_cong_discd_ind[1])
		print_string_val(child, "sl_cong_discd_ind[1]", s->sl_cong_discd_ind[1]);
	if (s->sl_cong_discd_ind[2])
		print_string_val(child, "sl_cong_discd_ind[2]", s->sl_cong_discd_ind[2]);
	if (s->sl_cong_discd_ind[3])
		print_string_val(child, "sl_cong_discd_ind[3]", s->sl_cong_discd_ind[3]);
}

const char *
oos_string(sl_ulong reason)
{
	switch (reason) {
	case SL_FAIL_UNSPECIFIED:
		return ("!out of service (unspec)");
	case SL_FAIL_CONG_TIMEOUT:
		return ("!out of service (T6)");
	case SL_FAIL_ACK_TIMEOUT:
		return ("!out of service (T7)");
	case SL_FAIL_ABNORMAL_BSNR:
		return ("!out of service (BSNR)");
	case SL_FAIL_ABNORMAL_FIBR:
		return ("!out of service (FIBR)");
	case SL_FAIL_SUERM_EIM:
		return ("!out of service (SUERM)");
	case SL_FAIL_ALIGNMENT_NOT_POSSIBLE:
		return ("!out of service (AERM)");
	case SL_FAIL_RECEIVED_SIO:
		return ("!out of service (SIO)");
	case SL_FAIL_RECEIVED_SIN:
		return ("!out of service (SIN)");
	case SL_FAIL_RECEIVED_SIE:
		return ("!out of service (SIE)");
	case SL_FAIL_RECEIVED_SIOS:
		return ("!out of service (SIOS)");
	case SL_FAIL_T1_TIMEOUT:
		return ("!out of service (T1)");
	default:
		return ("!out of service (\?\?\?)");
	}
}

const char *
prim_string(int prim)
{
	switch (prim) {
	case SL_REMOTE_PROCESSOR_OUTAGE_IND:
		return ("!rpo");
	case SL_REMOTE_PROCESSOR_RECOVERED_IND:
		return ("!rpr");
	case SL_IN_SERVICE_IND:
		return ("!in service");
	case SL_OUT_OF_SERVICE_IND:
		return ("!out of service");
	case SL_PDU_IND:
		return ("!msu");
	case SL_LINK_CONGESTED_IND:
		return ("!congested");
	case SL_LINK_CONGESTION_CEASED_IND:
		return ("!congestion ceased");
	case SL_RETRIEVED_MESSAGE_IND:
		return ("!retrieved message");
	case SL_RETRIEVAL_COMPLETE_IND:
		return ("!retrieval complete");
	case SL_RB_CLEARED_IND:
		return ("!rb cleared");
	case SL_BSNT_IND:
		return ("!bsnt");
	case SL_RTB_CLEARED_IND:
		return ("!rtb cleared");
	case LMI_INFO_ACK:
		return ("!info ack");
	case LMI_OK_ACK:
		return ("!ok ack");
	case LMI_ERROR_ACK:
		return ("!error ack");
	case LMI_ENABLE_CON:
		return ("!enable con");
	case LMI_DISABLE_CON:
		return ("!disable con");
	case LMI_ERROR_IND:
		return ("!error ind");
	case LMI_STATS_IND:
		return ("!stats ind");
	case LMI_EVENT_IND:
		return ("!event ind");
	default:
		return ("?_????_??? -----");
	}
}

void
print_simple(int child, const char *msgs[])
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child]);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_simple_int(int child, const char *msgs[], int val)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], val);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_double_int(int child, const char *msgs[], int val, int val2)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], val, val2);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_triple_int(int child, const char *msgs[], int val, int val2, int val3)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], val, val2, val3);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_simple_string(int child, const char *msgs[], const char *string)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], string);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_string_state(int child, const char *msgs[], const char *string)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], string, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_triple_string(int child, const char *msgs[], const char *string)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], "", child, state);
	fprintf(stdout, msgs[child], string, child, state);
	fprintf(stdout, msgs[child], "", child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_more(int child)
{
	show = 1;
}

void
print_less(int child)
{
	static const char *msgs[] = {
		"         . %1$6.6s . | <------         .         ------> :                    [%2$d:%3$03d]\n",
		"                    : <------         .         ------> | . %1$-6.6s .         [%2$d:%3$03d]\n",
		"                    : <------         .      ------> |  : . %1$-6.6s .         [%2$d:%3$03d]\n",
		"         . %1$6.6s . : <------         .      ------> :> : . %1$-6.6s .         [%2$d:%3$03d]\n",
	};

	if (show && verbose > 0)
		print_triple_string(child, msgs, "(more)");
	show = 0;
}

void
print_pipe(int child)
{
	static const char *msgs[] = {
		"  pipe()      ----->v  v<------------------------------>v                   \n",
		"                    v  v<------------------------------>v<-----     pipe()  \n",
		"                    .  .                                .                   \n",
	};

	if (show && verbose > 3)
		print_simple(child, msgs);
}

void
print_open(int child, const char *name)
{
	static const char *msgs[] = {
		"  open()      ----->v %-30.30s    .                   \n",
		"                    | %-30.30s    v<-----     open()  \n",
		"                    | %-30.30s v<-+------     open()  \n",
		"                    . %-30.30s .  .                   \n",
	};

	if (show && verbose > 3)
		print_simple_string(child, msgs, name);
}

void
print_close(int child)
{
	static const char *msgs[] = {
		"  close()     ----->X                                   |                   \n",
		"                    .                                   X<-----    close()  \n",
		"                    .                                X<-+------    close()  \n",
		"                    .                                .  .                   \n",
	};

	if (show && verbose > 3)
		print_simple(child, msgs);
}

void
print_preamble(int child)
{
	static const char *msgs[] = {
		"--------------------+-------------Preamble--------------+                   \n",
		"                    +-------------Preamble--------------+-------------------\n",
		"                    +-------------Preamble-----------+--+-------------------\n",
		"--------------------+-------------Preamble--------------+-------------------\n",
	};

	if (verbose > 0)
		print_simple(child, msgs);
}

void
print_failure(int child, const char *string)
{
	static const char *msgs[] = {
		"....................|%-32.32s   |                    [%d:%03d]\n",
		"                    |%-32.32s   |................... [%d:%03d]\n",
		"                    |%-32.32s|...................... [%d:%03d]\n",
		"....................|%-32.32s...|................... [%d:%03d]\n",
	};

	if (string && strnlen(string, 32) > 0 && verbose > 0)
		print_string_state(child, msgs, string);
}

void
print_notapplicable(int child)
{
	static const char *msgs[] = {
		"X-X-X-X-X-X-X-X-X-X-|X-X-X-X-X NOT APPLICABLE -X-X-X-X-X|                    [%d:%03d]\n",
		"                    |X-X-X-X-X NOT APPLICABLE -X-X-X-X-X|X-X-X-X-X-X-X-X-X-X [%d:%03d]\n",
		"                    |X-X-X-X-X NOT APPLICABLE -X-X-X-|-X|X-X-X-X-X-X-X-X-X-X [%d:%03d]\n",
		"X-X-X-X-X-X-X-X-X-X-|X-X-X-X-X NOT APPLICABLE -X-X-X-X-X|X-X-X-X-X-X-X-X-X-X [%d:%03d]\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, state);
	print_failure(child, failure_string);
}

void
print_skipped(int child)
{
	static const char *msgs[] = {
		"::::::::::::::::::::|:::::::::::: SKIPPED ::::::::::::::|                    [%d:%03d]\n",
		"                    |:::::::::::: SKIPPED ::::::::::::::|::::::::::::::::::: [%d:%03d]\n",
		"                    |:::::::::::: SKIPPED :::::::::::|::|::::::::::::::::::: [%d:%03d]\n",
		"::::::::::::::::::::|:::::::::::: SKIPPED ::::::::::::::|::::::::::::::::::: [%d:%03d]\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, state);
	print_failure(child, failure_string);
}

void
print_inconclusive(int child)
{
	static const char *msgs[] = {
		"????????????????????|?????????? INCONCLUSIVE ???????????|                    [%d:%03d]\n",
		"                    |?????????? INCONCLUSIVE ???????????|??????????????????? [%d:%03d]\n",
		"                    |?????????? INCONCLUSIVE ????????|??|??????????????????? [%d:%03d]\n",
		"????????????????????|?????????? INCONCLUSIVE ???????????|??????????????????? [%d:%03d]\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, state);
	print_failure(child, failure_string);
}

void
print_test(int child)
{
	static const char *msgs[] = {
		"--------------------+---------------Test----------------+                   \n",
		"                    +---------------Test----------------+-------------------\n",
		"                    +---------------Test-------------+--+-------------------\n",
		"--------------------+---------------Test----------------+-------------------\n",
	};

	if (verbose > 0)
		print_simple(child, msgs);
}

void
print_failed(int child)
{
	static const char *msgs[] = {
		"XXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX FAILED XXXXXXXXXXXXXX|                    [%d:%03d]\n",
		"                    |XXXXXXXXXXXXX FAILED XXXXXXXXXXXXXX|XXXXXXXXXXXXXXXXXXX [%d:%03d]\n",
		"                    |XXXXXXXXXXXXX FAILED XXXXXXXXXXX|XX|XXXXXXXXXXXXXXXXXXX [%d:%03d]\n",
		"XXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX FAILED XXXXXXXXXXXXXX|XXXXXXXXXXXXXXXXXXX [%d:%03d]\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, state);
	print_failure(child, failure_string);
}

void
print_script_error(int child)
{
	static const char *msgs[] = {
		"####################|########### SCRIPT ERROR ##########|                    [%d:%03d]\n",
		"                    |########### SCRIPT ERROR ##########|################### [%d:%03d]\n",
		"                    |########### SCRIPT ERROR #######|##|################### [%d:%03d]\n",
		"####################|########### SCRIPT ERROR ##########|################### [%d:%03d]\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, state);
	print_failure(child, failure_string);
}

void
print_passed(int child)
{
	static const char *msgs[] = {
		"********************|************* PASSED **************|                    [%d:%03d]\n",
		"                    |************* PASSED **************|******************* [%d:%03d]\n",
		"                    |************* PASSED ***********|**|******************* [%d:%03d]\n",
		"********************|************* PASSED **************|******************* [%d:%03d]\n",
	};

	if (verbose > 2)
		print_double_int(child, msgs, child, state);
	print_failure(child, failure_string);
}

void
print_postamble(int child)
{
	static const char *msgs[] = {
		"--------------------+-------------Postamble-------------+                   \n",
		"                    +-------------Postamble-------------+-------------------\n",
		"                    +-------------Postamble----------+--+-------------------\n",
		"--------------------+-------------Postamble-------------+-------------------\n",
	};

	if (verbose > 0)
		print_simple(child, msgs);
}

void
print_test_end(int child)
{
	static const char *msgs[] = {
		"--------------------+-----------------------------------+                   \n",
		"                    +-----------------------------------+-------------------\n",
		"                    +--------------------------------+--+-------------------\n",
		"--------------------+-----------------------------------+-------------------\n",
	};

	if (verbose > 0)
		print_simple(child, msgs);
}

void
print_terminated(int child, int signal)
{
	static const char *msgs[] = {
		"@@@@@@@@@@@@@@@@@@@@|@@@@@@@@@@@ TERMINATED @@@@@@@@@@@@|                    {%d:%03d}\n",
		"                    |@@@@@@@@@@@ TERMINATED @@@@@@@@@@@@|@@@@@@@@@@@@@@@@@@@ {%d:%03d}\n",
		"                    |@@@@@@@@@@@ TERMINATED @@@@@@@@@|@@|@@@@@@@@@@@@@@@@@@@ {%d:%03d}\n",
		"@@@@@@@@@@@@@@@@@@@@|@@@@@@@@@@@ TERMINATED @@@@@@@@@@@@|@@@@@@@@@@@@@@@@@@@ {%d:%03d}\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, signal);
}

void
print_stopped(int child, int signal)
{
	static const char *msgs[] = {
		"&&&&&&&&&&&&&&&&&&&&|&&&&&&&&&&&& STOPPED &&&&&&&&&&&&&&|                    {%d:%03d}\n",
		"                    |&&&&&&&&&&&& STOPPED &&&&&&&&&&&&&&|&&&&&&&&&&&&&&&&&&& {%d:%03d}\n",
		"                    |&&&&&&&&&&&& STOPPED &&&&&&&&&&&|&&|&&&&&&&&&&&&&&&&&&& {%d:%03d}\n",
		"&&&&&&&&&&&&&&&&&&&&|&&&&&&&&&&&& STOPPED &&&&&&&&&&&&&&|&&&&&&&&&&&&&&&&&&& {%d:%03d}\n",
	};

	if (verbose > 0)
		print_double_int(child, msgs, child, signal);
}

void
print_timeout(int child)
{
	static const char *msgs[] = {
		"++++++++++++++++++++|++++++++++++ TIMEOUT! +++++++++++++|                    [%d:%03d]\n",
		"                    |++++++++++++ TIMEOUT! +++++++++++++|+++++++++++++++++++ [%d:%03d]\n",
		"                    |++++++++++++ TIMEOUT! ++++++++++|++|+++++++++++++++++++ [%d:%03d]\n",
		"++++++++++++++++++++|++++++++++++ TIMEOUT! +++++++++++++|+++++++++++++++++++ [%d:%03d]\n",
	};

	if (show_timeout || verbose > 1) {
		print_double_int(child, msgs, child, state);
		show_timeout--;
	}
}

void
print_nothing(int child)
{
	static const char *msgs[] = {
		"- - - - - - - - - - |- - - - - - -nothing! - - - - - - -|                    [%d:%03d]\n",
		"                    |- - - - - - -nothing! - - - - - - -|- - - - - - - - - - [%d:%03d]\n",
		"                    |- - - - - - -nothing! - - - - - | -|- - - - - - - - - - [%d:%03d]\n",
		"- - - - - - - - - - |- - - - - - -nothing! - - - - - - -|- - - - - - - - - - [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_double_int(child, msgs, child, state);
}

void
print_syscall(int child, const char *command)
{
	static const char *msgs[] = {
		"%-14s----->|                                   |                    [%d:%03d]\n",
		"                    |                                   |<---%-14s  [%d:%03d]\n",
		"                    |                                |<-+----%-14s  [%d:%03d]\n",
		"                    |          %-14s           |                    [%d:%03d]\n",
	};

	if ((verbose && show) || (verbose > 5 && show_msg))
		print_string_state(child, msgs, command);
}

void
print_tx_prim(int child, const char *command)
{
	static const char *msgs[] = {
		"--%16s->| - - - - - - - - - - - - - - - - ->|                    [%d:%03d]\n",
		"                    |<- - - - - - - - - - - - - - - - - |<-%16s- [%d:%03d]\n",
		"                    |<- - - - - - - - - - - - - - - -|<----%16s- [%d:%03d]\n",
		"                    |         %-16s          |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, command);
}

void
print_rx_prim(int child, const char *command)
{
	static const char *msgs[] = {
		"<-%16s--|<- - - - - - - - - - - - - - - - - |                    [%d:%03d]\n",
		"                    |- - - - - - - - - - - - - - - - - >|--%16s> [%d:%03d]\n",
		"                    |- - - - - - - - - - - - - - - ->|--+--%16s> [%d:%03d]\n",
		"                    |         <%16s>        |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, command);
}

void
print_string_state_sn(int child, const char *msgs[], const char *string, uint32_t bsn, uint32_t fsn)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], string, bsn, fsn, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_tx_msg_sn(int child, const char *string, uint32_t bsn, uint32_t fsn)
{
	static const char *msgs[] = {
		"%1$20.20s| ---------[%3$06X,%2$06X]--------> |                    [%4$d:%5$03d]\n",
		"                    | <--------[%2$06X,%3$06X]--------- |%1$-20.20s[%4$d:%5$03d]\n",
		"                    | <--------[%2$06X,%3$06X]------ |  |%1$-20.20s[%4$d:%5$03d]\n",
		"                    |      <%1$-20.20s>       |                    [%4$d:%5$03d]\n",
	};

	if (show && verbose > 0)
		print_string_state_sn(child, msgs, string, bsn, fsn);
}

void
print_rx_msg_sn(int child, const char *string, uint32_t bsn, uint32_t fsn)
{
	static const char *msgs[] = {
		"%1$20.20s| <--------[%2$06X,%3$06X]--------- |                    [%4$d:%5$03d]\n",
		"                    | ---------[%3$06X,%2$06X]--------> |%1$-20.20s[%4$d:%5$03d]\n",
		"                    | ---------[%3$06X,%2$06X]-----> |   %1$-20.20s[%4$d:%5$03d]\n",
		"                    |       <%1$20.20s>      |                    [%4$d:%5$03d]\n",
	};

	if (show && verbose > 0)
		print_string_state_sn(child, msgs, string, bsn, fsn);
}

void
print_tx_msg(int child, const char *string)
{
	static const char *msgs[] = {
		"%20.20s| --------------------------------> |                    [%d:%03d]\n",
		"                    | <-------------------------------- |%-20.20s[%d:%03d]\n",
		"                    | <----------------------------- |  |%-20.20s[%d:%03d]\n",
		"                    |      <%-20.20s>       |                    [%d:%03d]\n",
	};

	if (show && verbose > 0) {
		if (fsn[child] != 0x7f || bsn[child] != 0x7f || fib[child] != 0x80 || bib[child] != 0x80)
			print_tx_msg_sn(child, string, bib[child] | bsn[child], fib[child] | fsn[child]);
		else
			print_string_state(child, msgs, string);
	}
}

void
print_rx_msg(int child, const char *string)
{
	static const char *msgs[] = {
		"%20.20s| <-------------------------------- |                    [%d:%03d]\n",
		"                    | --------------------------------> |%-20.20s[%d:%03d]\n",
		"                    | -----------------------------> |   %-20.20s[%d:%03d]\n",
		"                    |       <%20.20s>      |                    [%d:%03d]\n",
	};

	if (show && verbose > 0) {
		int other = (child + 1) % 2;

		if (fsn[other] != 0x7f || bsn[other] != 0x7f || fib[other] != 0x80 || bib[other] != 0x80)
			print_rx_msg_sn(child, string, bib[other] | bsn[other], fib[other] | fsn[other]);
		else
			print_string_state(child, msgs, string);
	}
}

void
print_ack_prim(int child, const char *command)
{
	static const char *msgs[] = {
		"<-%16s-/|                                   |                    [%d:%03d]\n",
		"                    |                                   |\\-%16s> [%d:%03d]\n",
		"                    |                                |\\-+--%16s> [%d:%03d]\n",
		"                    |         <%16s>        |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, command);
}

void
print_long_state(int child, const char *msgs[], long value)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], value, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_no_prim(int child, long prim)
{
	static const char *msgs[] = {
		"????%4ld????  ?----?| ?- - - - - - - - - - - - - - - -? |                     [%d:%03d]\n",
		"                    | ?- - - - - - - - - - - - - - - -? |?--? ????%4ld????    [%d:%03d]\n",
		"                    | ?- - - - - - - -- - - - - - -? |?-+---? ????%4ld????    [%d:%03d]\n",
		"                    | ?- - - - - - -%4ld- - - - - -? |  |                     [%d:%03d]\n",
	};

	if (verbose > 1)
		print_long_state(child, msgs, prim);
}

void
print_signal(int child, int signum)
{
	static const char *msgs[] = {
		">>>>>>>>>>>>>>>>>>>>|>>>>>>>>>>>> %-8.8s <<<<<<<<<<<<<|                    [%d:%03d]\n",
		"                    |>>>>>>>>>>>> %-8.8s <<<<<<<<<<<<<|<<<<<<<<<<<<<<<<<<< [%d:%03d]\n",
		"                    |>>>>>>>>>>>> %-8.8s <<<<<<<<<<|<<|<<<<<<<<<<<<<<<<<<< [%d:%03d]\n",
		">>>>>>>>>>>>>>>>>>>>|>>>>>>>>>>>> %-8.8s <<<<<<<<<<<<<|<<<<<<<<<<<<<<<<<<< [%d:%03d]\n",
	};

	if (verbose > 0)
		print_string_state(child, msgs, signal_string(signum));
}

void
print_double_string_state(int child, const char *msgs[], const char *string1, const char *string2)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], string1, string2, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_command_info(int child, const char *command, const char *info)
{
	static const char *msgs[] = {
		"%1$-14s----->|         %2$-16.16s          |                    [%3$d:%4$03d]\n",
		"                    |         %2$-16.16s          |<---%1$-14s  [%3$d:%4$03d]\n",
		"                    |         %2$-16.16s       |<-+----%1$-14s  [%3$d:%4$03d]\n",
		"                    | %1$-14s %2$-16.16s|  |                    [%3$d:%4$03d]\n",
	};

	if (show && verbose > 3)
		print_double_string_state(child, msgs, command, info);
}

void
print_string_int_state(int child, const char *msgs[], const char *string, int val)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], string, val, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_tx_data(int child, const char *command, size_t bytes)
{
	static const char *msgs[] = {
		"--%1$16s->| - %2$5d bytes - - - - - - - - - ->|                    [%3$d:%4$03d]\n",
		"                    |<- %2$5d bytes - - - - - - - - - - |<-%1$16s- [%3$d:%4$03d]\n",
		"                    |<- %2$5d bytes - - - - - - - - -|<-+ -%1$16s- [%3$d:%4$03d]\n",
		"                    | - %2$5d bytes %1$16s    |                    [%3$d:%4$03d]\n",
	};

	if ((verbose && show) || verbose > 4)
		print_string_int_state(child, msgs, command, bytes);
}

void
print_rx_data(int child, const char *command, size_t bytes)
{
	static const char *msgs[] = {
		"<-%1$16s--|<- -%2$4d bytes- - - - - - - - - - -|                    [%3$d:%4$03d]\n",
		"                    |- - %2$4d bytes- - - - - - - - - - >|--%1$16s> [%3$d:%4$03d]\n",
		"                    |- - %2$4d bytes- - - - - - - - ->|--+--%1$16s> [%3$d:%4$03d]\n",
		"                    |- - %2$4d bytes %1$16s    |                    [%3$d:%4$03d]\n",
	};

	if ((verbose && show) || (verbose > 5 && show_msg))
		print_string_int_state(child, msgs, command, bytes);
}

void
print_errno(int child, long error)
{
	static const char *msgs[] = {
		"  %-14s<--/|                                   |                    [%d:%03d]\n",
		"                    |                                   |\\-->%14s  [%d:%03d]\n",
		"                    |                                |\\-+--->%14s  [%d:%03d]\n",
		"                    |          [%14s]         |                    [%d:%03d]\n",
	};

	if ((verbose > 4 && show) || (verbose > 5 && show_msg))
		print_string_state(child, msgs, errno_string(error));
}

void
print_success(int child)
{
	static const char *msgs[] = {
		"  ok          <----/|                                   |                    [%d:%03d]\n",
		"                    |                                   |\\---->         ok   [%d:%03d]\n",
		"                    |                                |\\-+----->         ok   [%d:%03d]\n",
		"                    |                 ok                |                    [%d:%03d]\n",
	};

	if ((verbose > 4 && show) || (verbose > 5 && show_msg))
		print_double_int(child, msgs, child, state);
}

void
print_success_value(int child, int value)
{
	static const char *msgs[] = {
		"  %10d  <----/|                                   |                    [%d:%03d]\n",
		"                    |                                   |\\---->  %10d  [%d:%03d]\n",
		"                    |                                |\\-+----->  %10d  [%d:%03d]\n",
		"                    |            [%10d]           |                    [%d:%03d]\n",
	};

	if ((verbose > 4 && show) || (verbose > 5 && show_msg))
		print_triple_int(child, msgs, value, child, state);
}

void
print_int_string_state(int child, const char *msgs[], const int value, const char *string)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], value, string, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_poll_value(int child, int value, short revents)
{
	static const char *msgs[] = {
		"  %1$10d  <----/| %2$-30.30s    |                    [%3$d:%4$03d]\n",
		"                    | %2$-30.30s    |\\---->  %1$10d  [%3$d:%4$03d]\n",
		"                    | %2$-30.30s |\\-+----->  %1$10d  [%3$d:%4$03d]\n",
		"                    | %2$-20.20s [%1$10d] |                    [%3$d:%4$03d]\n",
	};

	if (show && verbose > 3)
		print_int_string_state(child, msgs, value, poll_events_string(revents));
}

void
print_ti_ioctl(int child, int cmd, intptr_t arg)
{
	static const char *msgs[] = {
		"--ioctl(2)--------->|       %16s            |                    [%d:%03d]\n",
		"                    |       %16s            |<---ioctl(2)------  [%d:%03d]\n",
		"                    |       %16s         |<-+----ioctl(2)------  [%d:%03d]\n",
		"                    |       %16s ioctl(2)   |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, ioctl_string(cmd, arg));
}

void
print_ioctl(int child, int cmd, intptr_t arg)
{
	print_command_info(child, "ioctl(2)------", ioctl_string(cmd, arg));
}

void
print_poll(int child, short events)
{
	print_command_info(child, "poll(2)-------", poll_string(events));
}

void
print_datcall(int child, const char *command, size_t bytes)
{
	static const char *msgs[] = {
		"  %1$16s->| - %2$5d bytes - - - - - - - - - ->|                    [%3$d:%4$03d]\n",
		"                    |<- %2$5d bytes - - - - - - - - - - |<-%1$16s  [%3$d:%4$03d]\n",
		"                    |<- %2$5d bytes - - - - - - - - -|<-+--%1$16s  [%3$d:%4$03d]\n",
		"                    | - %2$5d bytes %1$16s    |                    [%3$d:%4$03d]\n",
	};

	if ((verbose > 4 && show) || (verbose > 5 && show_msg))
		print_string_int_state(child, msgs, command, bytes);
}

void
print_libcall(int child, const char *command)
{
	static const char *msgs[] = {
		"  %-16s->|                                   |                    [%d:%03d]\n",
		"                    |                                   |<-%16s  [%d:%03d]\n",
		"                    |                                |<-+--%16s  [%d:%03d]\n",
		"                    |        [%16s]         |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, command);
}

#if 0
void
print_terror(int child, long error, long terror)
{
	static const char *msgs[] = {
		"  %-14s<--/|                                   |                    [%d:%03d]\n",
		"                    |                                   |\\-->%14s  [%d:%03d]\n",
		"                    |                                |\\-+--->%14s  [%d:%03d]\n",
		"                    |          [%14s]         |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, t_errno_string(terror, error));
}
#endif

#if 0
void
print_tlook(int child, int tlook)
{
	static const char *msgs[] = {
		"  %-14s<--/|                                   |                    [%d:%03d]\n",
		"                    |                                   |\\-->%14s  [%d:%03d]\n",
		"                    |                                |\\-|--->%14s  [%d:%03d]\n",
		"                    |          [%14s]         |                    [%d:%03d]\n",
	};

	if (show && verbose > 1)
		print_string_state(child, msgs, t_look_string(tlook));
}
#endif

void
print_expect(int child, int want)
{
	static const char *msgs[] = {
		" (%-16s) |- - - - - -[Expected]- - - - - - - |                    [%d:%03d]\n",
		"                    |- - - - - -[Expected]- - - - - - - | (%-16s) [%d:%03d]\n",
		"                    |- - - - - -[Expected]- - - - - -|- | (%-16s) [%d:%03d]\n",
		"                    |- [Expected %-16s ] - - |                    [%d:%03d]\n",
	};

	if (verbose > 0 && show)
		print_string_state(child, msgs, event_string(child, want));
}

void
print_string(int child, const char *string)
{
	static const char *msgs[] = {
		"%-20s|                                   |                    \n",
		"                    |                                   |%-20s\n",
		"                    |                                |   %-20s\n",
		"                    |       %-20s        |                    \n",
	};

	if (show && verbose > 0)
		print_simple_string(child, msgs, string);
}

void
print_string_val(int child, const char *string, ulong val)
{
	static const char *msgs[] = {
		"%1$20.20s|          %2$15u          |                    \n",
		"                    |          %2$15u          |%1$-20.20s\n",
		"                    |          %2$15u       |   %1$-20.20s\n",
		"                    |%1$-20.20s%2$15u|                    \n",
	};

	if (show && verbose > 0) {
		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, msgs[child], string, val);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}
}

void
print_command_state(int child, const char *string)
{
	static const char *msgs[] = {
		"%20s|                                   |                    [%d:%03d]\n",
		"                    |                                   |%-20s[%d:%03d]\n",
		"                    |                                |   %-20s[%d:%03d]\n",
		"                    |       %-20s        |                    [%d:%03d]\n",
	};

	if (show && verbose > 0)
		print_string_state(child, msgs, string);
}

void
print_time_state(int child, const char *msgs[], ulong time)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], time, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_waiting(int child, ulong time)
{
	static const char *msgs[] = {
		"/ / / / / / / / / / | / / / Waiting %03lu seconds / / / / |                    [%d:%03d]\n",
		"                    | / / / Waiting %03lu seconds / / / / | / / / / / / / / /  [%d:%03d]\n",
		"                    | / / / Waiting %03lu seconds / / /|/ | / / / / / / / / /  [%d:%03d]\n",
		"/ / / / / / / / / / | / / / Waiting %03lu seconds / / / / | / / / / / / / / /  [%d:%03d]\n",
	};

	if (verbose > 1 && show)
		print_time_state(child, msgs, time);
}

void
print_float_state(int child, const char *msgs[], float time)
{
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, msgs[child], time, child, state);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

void
print_mwaiting(int child, struct timespec *time)
{
	static const char *msgs[] = {
		"/ / / / / / / / / / | / / Waiting %8.4f seconds/ / / |                    [%d:%03d]\n",
		"                    | / / Waiting %8.4f seconds/ / / | / / / / / / / / /  [%d:%03d]\n",
		"                    | / / Waiting %8.4f seconds/ /|/ / / / / / / / / / /  [%d:%03d]\n",
		"/ / / / / / / / / / | / / Waiting %8.4f seconds/ / / | / / / / / / / / /  [%d:%03d]\n",
	};

	if (verbose > 1 && show) {
		float delay;

		delay = time->tv_nsec;
		delay = delay / 1000000000;
		delay = delay + time->tv_sec;
		print_float_state(child, msgs, delay);
	}
}

#if 0
void
print_opt_level(int child, struct t_opthdr *oh)
{
	char *level = level_string(oh);

	if (level)
		print_string(child, level);
}

void
print_opt_name(int child, struct t_opthdr *oh)
{
	char *name = name_string(oh);

	if (name)
		print_string(child, name);
}

void
print_opt_status(int child, struct t_opthdr *oh)
{
	char *status = status_string(oh);

	if (status)
		print_string(child, status);
}

void
print_opt_length(int child, struct t_opthdr *oh)
{
	int len = oh->len - sizeof(*oh);

	if (len) {
		char buf[32];

		snprintf(buf, sizeof(buf), "(len=%d)", len);
		print_string(child, buf);
	}
}
void
print_opt_value(int child, struct t_opthdr *oh)
{
	char *value = value_string(child, oh);

	if (value)
		print_string(child, value);
}
#endif

#if 0
void
print_options(int child, const char *cmd_buf, size_t qos_ofs, size_t qos_len)
{
	unsigned char *qos_ptr = (unsigned char *) (cmd_buf + qos_ofs);
	union N_qos_ip_types *qos = (union N_qos_ip_types *) qos_ptr;
	char buf[64];

	if (verbose < 3 || !show)
		return;
	if (qos_len) {
		switch (qos->n_qos_type) {
		case N_QOS_SEL_CONN_IP:
			snprintf(buf, sizeof(buf), "N_QOS_SEL_CONN_IP:");
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " protocol = %ld,", (long) qos->n_qos_sel_conn.protocol);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " priority = %ld,", (long) qos->n_qos_sel_conn.priority);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " ttl = %ld,", (long) qos->n_qos_sel_conn.ttl);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " tos = %ld,", (long) qos->n_qos_sel_conn.tos);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " mtu = %ld,", (long) qos->n_qos_sel_conn.mtu);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " saddr = 0x%x,", (unsigned int) ntohl(qos->n_qos_sel_conn.saddr));
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " daddr = 0x%x,", (unsigned int) ntohl(qos->n_qos_sel_conn.daddr));
			print_string(child, buf);
			break;

		case N_QOS_SEL_UD_IP:
			snprintf(buf, sizeof(buf), "N_QOS_SEL_UD_IP: ");
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " protocol = %ld,", (long) qos->n_qos_sel_ud.protocol);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " priority = %ld,", (long) qos->n_qos_sel_ud.priority);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " ttl = %ld,", (long) qos->n_qos_sel_ud.ttl);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " tos = %ld,", (long) qos->n_qos_sel_ud.tos);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " saddr = 0x%x,", (unsigned int) ntohl(qos->n_qos_sel_ud.saddr));
			print_string(child, buf);
			break;

		case N_QOS_SEL_INFO_IP:
			snprintf(buf, sizeof(buf), "N_QOS_SEL_INFO_IP: ");
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " protocol = %ld,", (long) qos->n_qos_sel_info.protocol);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " priority = %ld,", (long) qos->n_qos_sel_info.priority);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " ttl = %ld,", (long) qos->n_qos_sel_info.ttl);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " tos = %ld,", (long) qos->n_qos_sel_info.tos);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " mtu = %ld,", (long) qos->n_qos_sel_info.mtu);
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " saddr = 0x%x,", (unsigned int) ntohl(qos->n_qos_sel_info.saddr));
			print_string(child, buf);
			snprintf(buf, sizeof(buf), " daddr = 0x%x,", (unsigned int) ntohl(qos->n_qos_sel_info.daddr));
			print_string(child, buf);
			break;

		case N_QOS_RANGE_INFO_IP:
			snprintf(buf, sizeof(buf), "N_QOS_RANGE_INFO_IP: ");
			print_string(child, buf);
			break;

		default:
			snprintf(buf, sizeof(buf), "(unknown qos structure %lu)\n", (ulong) qos->n_qos_type);
			print_string(child, buf);
			break;
		}
	} else {
		snprintf(buf, sizeof(buf), "(no qos)");
		print_string(child, buf);
	}
}
#endif

#if 0
int
ip_n_open(const char *name, int *fdp)
{
	int fd;

	for (;;) {
		print_open(fdp, name);
		if ((fd = open(name, O_NONBLOCK | O_RDWR)) >= 0) {
			*fdp = fd;
			print_success(fd);
			return (__RESULT_SUCCESS);
		}
		print_errno(fd, (last_errno = errno));
		if (last_errno == EINTR || last_errno == ERESTART)
			continue;
		return (__RESULT_FAILURE);
	}
}

int
ip_close(int *fdp)
{
	int fd = *fdp;

	*fdp = 0;
	for (;;) {
		print_close(fdp);
		if (close(fd) >= 0) {
			print_success(fd);
			return __RESULT_SUCCESS;
		}
		print_errno(fd, (last_errno = errno));
		if (last_errno == EINTR || last_errno == ERESTART)
			continue;
		return __RESULT_FAILURE;
	}
}

int
ip_datack_req(int fd)
{
	int ret;

	data.len = -1;
	ctrl.len = sizeof(cmd.npi.datack_req) + sizeof(qos_data);
	cmd.prim = N_DATACK_REQ;
	bcopy(&qos_data, cbuf + sizeof(cmd.npi.datack_req), sizeof(qos_data));
	ret = put_msg(fd, 0, MSG_BAND, 0);
	return (ret);
}
#endif

/*
 *  -------------------------------------------------------------------------
 *
 *  Driver actions.
 *
 *  -------------------------------------------------------------------------
 */

int
test_waitsig(int child)
{
	int signum;
	sigset_t set;

	sigemptyset(&set);
	while ((signum = last_signum) == 0)
		sigsuspend(&set);
	print_signal(child, signum);
	return (__RESULT_SUCCESS);

}

int
test_ioctl(int child, int cmd, intptr_t arg)
{
	print_ioctl(child, cmd, arg);
	for (;;) {
		if ((last_retval = ioctl(test_fd[child], cmd, arg)) == -1) {
			print_errno(child, (last_errno = errno));
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			return (__RESULT_FAILURE);
		}
		if (show && verbose > 3)
			print_success_value(child, last_retval);
		return (__RESULT_SUCCESS);
	}
}

int
test_insertfd(int child, int resfd, int offset, struct strbuf *ctrl, struct strbuf *data, int flags)
{
	struct strfdinsert fdi;

	if (ctrl) {
		fdi.ctlbuf.maxlen = ctrl->maxlen;
		fdi.ctlbuf.len = ctrl->len;
		fdi.ctlbuf.buf = ctrl->buf;
	} else {
		fdi.ctlbuf.maxlen = -1;
		fdi.ctlbuf.len = 0;
		fdi.ctlbuf.buf = NULL;
	}
	if (data) {
		fdi.databuf.maxlen = data->maxlen;
		fdi.databuf.len = data->len;
		fdi.databuf.buf = data->buf;
	} else {
		fdi.databuf.maxlen = -1;
		fdi.databuf.len = 0;
		fdi.databuf.buf = NULL;
	}
	fdi.flags = flags;
	fdi.fildes = resfd;
	fdi.offset = offset;
	if (show && verbose > 4) {
		int i;

		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, "fdinsert to %d: [%d,%d]\n", child, ctrl ? ctrl->len : -1, data ? data->len : -1);
		fprintf(stdout, "[");
		for (i = 0; i < (ctrl ? ctrl->len : 0); i++)
			fprintf(stdout, "%02X", (uint8_t) ctrl->buf[i]);
		fprintf(stdout, "]\n");
		fprintf(stdout, "[");
		for (i = 0; i < (data ? data->len : 0); i++)
			fprintf(stdout, "%02X", (uint8_t) data->buf[i]);
		fprintf(stdout, "]\n");
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}
	if (test_ioctl(child, I_FDINSERT, (intptr_t) &fdi) != __RESULT_SUCCESS)
		return __RESULT_FAILURE;
	return __RESULT_SUCCESS;
}

int
test_putmsg(int child, struct strbuf *ctrl, struct strbuf *data, int flags)
{
	print_datcall(child, "putmsg(2)-----", data ? data->len : -1);
	if ((last_retval = putmsg(test_fd[child], ctrl, data, flags)) == -1) {
		print_errno(child, (last_errno = errno));
		return (__RESULT_FAILURE);
	}
	print_success_value(child, last_retval);
	return (__RESULT_SUCCESS);
}

int
test_putpmsg(int child, struct strbuf *ctrl, struct strbuf *data, int band, int flags)
{
	if (flags & MSG_BAND || band) {
		if ((verbose > 3 && show) || (verbose > 5 && show_msg)) {
			int i;

			dummy = lockf(fileno(stdout), F_LOCK, 0);
			fprintf(stdout, "putpmsg to %d: [%d,%d]\n", child, ctrl ? ctrl->len : -1, data ? data->len : -1);
			fprintf(stdout, "[");
			for (i = 0; i < (ctrl ? ctrl->len : 0); i++)
				fprintf(stdout, "%02X", (uint8_t) ctrl->buf[i]);
			fprintf(stdout, "]\n");
			fprintf(stdout, "[");
			for (i = 0; i < (data ? data->len : 0); i++)
				fprintf(stdout, "%02X", (uint8_t) data->buf[i]);
			fprintf(stdout, "]\n");
			fflush(stdout);
			dummy = lockf(fileno(stdout), F_ULOCK, 0);
		}
		if (ctrl == NULL || data != NULL)
			print_datcall(child, "M_DATA----------", data ? data->len : 0);
		for (;;) {
			if ((last_retval = putpmsg(test_fd[child], ctrl, data, band, flags)) == -1) {
				if (last_errno == EINTR || last_errno == ERESTART)
					continue;
				print_errno(child, (last_errno = errno));
				return (__RESULT_FAILURE);
			}
			if ((verbose > 3 && show) || (verbose > 5 && show_msg))
				print_success_value(child, last_retval);
			return (__RESULT_SUCCESS);
		}
	} else {
		if ((verbose > 3 && show) || (verbose > 5 && show_msg)) {
			dummy = lockf(fileno(stdout), F_LOCK, 0);
			fprintf(stdout, "putmsg to %d: [%d,%d]\n", child, ctrl ? ctrl->len : -1, data ? data->len : -1);
			dummy = lockf(fileno(stdout), F_ULOCK, 0);
			fflush(stdout);
		}
		if (ctrl == NULL || data != NULL)
			print_datcall(child, "M_DATA----------", data ? data->len : 0);
		for (;;) {
			if ((last_retval = putmsg(test_fd[child], ctrl, data, flags)) == -1) {
				if (last_errno == EINTR || last_errno == ERESTART)
					continue;
				print_errno(child, (last_errno = errno));
				return (__RESULT_FAILURE);
			}
			if ((verbose > 3 && show) || (verbose > 5 && show_msg))
				print_success_value(child, last_retval);
			return (__RESULT_SUCCESS);
		}
	}
}

int
test_write(int child, const void *buf, size_t len)
{
	print_syscall(child, "write(2)------");
	for (;;) {
		if ((last_retval = write(test_fd[child], buf, len)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_writev(int child, const struct iovec *iov, int num)
{
	print_syscall(child, "writev(2)-----");
	for (;;) {
		if ((last_retval = writev(test_fd[child], iov, num)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_getmsg(int child, struct strbuf *ctrl, struct strbuf *data, int *flagp)
{
	print_syscall(child, "getmsg(2)-----");
	for (;;) {
		if ((last_retval = getmsg(test_fd[child], ctrl, data, flagp)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_getpmsg(int child, struct strbuf *ctrl, struct strbuf *data, int *bandp, int *flagp)
{
	print_syscall(child, "getpmsg(2)----");
	for (;;) {
		if ((last_retval = getpmsg(test_fd[child], ctrl, data, bandp, flagp)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_read(int child, void *buf, size_t count)
{
	print_syscall(child, "read(2)-------");
	for (;;) {
		if ((last_retval = read(test_fd[child], buf, count)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_readv(int child, const struct iovec *iov, int count)
{
	print_syscall(child, "readv(2)------");
	for (;;) {
		if ((last_retval = readv(test_fd[child], iov, count)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_ti_ioctl(int child, int cmd, intptr_t arg)
{
	if (cmd == I_STR && verbose > 3) {
		struct strioctl *icp = (struct strioctl *) arg;

		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, "ioctl from %d: cmd=%d, timout=%d, len=%d, dp=%p\n", child, icp->ic_cmd, icp->ic_timout, icp->ic_len, icp->ic_dp);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}
	print_ti_ioctl(child, cmd, arg);
	for (;;) {
		if ((last_retval = ioctl(test_fd[child], cmd, arg)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		if (show && verbose > 3)
			print_success_value(child, last_retval);
		break;
	}
	if (cmd == I_STR && show && verbose > 3) {
		struct strioctl *icp = (struct strioctl *) arg;

		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, "got ioctl from %d: cmd=%d, timout=%d, len=%d, dp=%p\n", child, icp->ic_cmd, icp->ic_timout, icp->ic_len, icp->ic_dp);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}
	if (last_retval == 0)
		return __RESULT_SUCCESS;
	if (verbose) {
		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, "***************ERROR: ioctl failed\n");
		if (show && verbose > 3)
			fprintf(stdout, "                    : %s; result = %d\n", __FUNCTION__, last_retval);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
		fflush(stdout);
	}
	return (__RESULT_FAILURE);
}

int
test_nonblock(int child)
{
	long flags;

	print_syscall(child, "fcntl(2)------");
	for (;;) {
		if ((flags = last_retval = fcntl(test_fd[child], F_GETFL)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	print_syscall(child, "fcntl(2)------");
	for (;;) {
		if ((last_retval = fcntl(test_fd[child], F_SETFL, flags | O_NONBLOCK)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_block(int child)
{
	long flags;

	print_syscall(child, "fcntl(2)------");
	for (;;) {
		if ((flags = last_retval = fcntl(test_fd[child], F_GETFL)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	print_syscall(child, "fcntl(2)------");
	for (;;) {
		if ((last_retval = fcntl(test_fd[child], F_SETFL, flags & ~O_NONBLOCK)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_isastream(int child)
{
	int result;

	print_syscall(child, "isastream(2)--");
	for (;;) {
		if ((result = last_retval = isastream(test_fd[child])) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_success_value(child, last_retval);
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_poll(int child, const short events, short *revents, long ms)
{
	struct pollfd pfd = {.fd = test_fd[child],.events = events,.revents = 0 };
	int result;

	print_poll(child, events);
	for (;;) {
		if ((result = last_retval = poll(&pfd, 1, ms)) == -1) {
			if (last_errno == EINTR || last_errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		}
		print_poll_value(child, last_retval, pfd.revents);
		if (last_retval == 1 && revents)
			*revents = pfd.revents;
		break;
	}
	return (__RESULT_SUCCESS);
}

int
test_pipe(int child)
{
	int fds[2];

	for (;;) {
		print_pipe(child);
		if (pipe(fds) >= 0) {
			test_fd[child + 0] = fds[0];
			test_fd[child + 1] = fds[1];
			print_success(child);
			return (__RESULT_SUCCESS);
		}
		if (last_errno == EINTR || last_errno == ERESTART)
			continue;
		print_errno(child, (last_errno = errno));
		return (__RESULT_FAILURE);
	}
}

int
test_fopen(int child, const char *name, int flags)
{
	int fd;

	print_open(child, name);
	if ((fd = open(name, flags)) >= 0) {
		print_success(child);
		return (fd);
	}
	print_errno(child, (last_errno = errno));
	return (fd);
}

int
test_fclose(int child, int fd)
{
	print_close(child);
	if (close(fd) >= 0) {
		print_success(child);
		return __RESULT_SUCCESS;
	}
	print_errno(child, (last_errno = errno));
	return __RESULT_FAILURE;
}

int
test_open(int child, const char *name, int flags)
{
	int fd;

	for (;;) {
		print_open(child, name);
		if ((fd = open(name, flags)) >= 0) {
			test_fd[child] = fd;
			print_success(child);
			return (__RESULT_SUCCESS);
		}
		if (last_errno == EINTR || last_errno == ERESTART)
			continue;
		print_errno(child, (last_errno = errno));
		return (__RESULT_FAILURE);
	}
}

int
test_close(int child)
{
	int fd = test_fd[child];

	test_fd[child] = 0;
	for (;;) {
		print_close(child);
		if (close(fd) >= 0) {
			print_success(child);
			return __RESULT_SUCCESS;
		}
		if (last_errno == EINTR || last_errno == ERESTART)
			continue;
		print_errno(child, (last_errno = errno));
		return __RESULT_FAILURE;
	}
}

int
test_push(int child, const char *name)
{
	if (show && verbose > 1)
		print_command_state(child, ":push");
	if (test_ioctl(child, I_PUSH, (intptr_t) name))
		return __RESULT_FAILURE;
	return __RESULT_SUCCESS;
}

int
test_pop(int child)
{
	if (show && verbose > 1)
		print_command_state(child, ":pop");
	if (test_ioctl(child, I_POP, (intptr_t) 0))
		return __RESULT_FAILURE;
	return __RESULT_SUCCESS;
}

/*
 *  -------------------------------------------------------------------------
 *
 *  Stream Initialization
 *
 *  -------------------------------------------------------------------------
 */

static int
stream_start(int child, int index)
{
	switch (child) {
	case 1:
	case 2:
	case 0:
		if (test_open(child, devname, O_RDWR) != __RESULT_SUCCESS)
			return __RESULT_FAILURE;
		if (test_ioctl(child, I_SRDOPT, (intptr_t) RMSGD) != __RESULT_SUCCESS)
			return __RESULT_FAILURE;
		return __RESULT_SUCCESS;
	default:
		return __RESULT_FAILURE;
	}
}

static int
stream_stop(int child)
{
	switch (child) {
	case 1:
	case 2:
	case 0:
		if (test_close(child) != __RESULT_SUCCESS)
			return __RESULT_FAILURE;
		return __RESULT_SUCCESS;
	default:
		return __RESULT_FAILURE;
	}
}

void
test_sleep(int child, unsigned long t)
{
	print_waiting(child, t);
	sleep(t);
}

void
test_msleep(int child, unsigned long m)
{
	struct timespec time;

	time.tv_sec = m / 1000;
	time.tv_nsec = (m % 1000) * 1000000;
	print_mwaiting(child, &time);
	nanosleep(&time, NULL);
}

/*
 *  -------------------------------------------------------------------------
 *
 *  Test harness initialization and termination.
 *
 *  -------------------------------------------------------------------------
 */

static int
begin_tests(int index)
{
	state = 0;
	if (stream_start(0, index) != __RESULT_SUCCESS)
		goto failure;
	state++;
	if (stream_start(1, index) != __RESULT_SUCCESS)
		goto failure;
	state++;
	if (stream_start(2, index) != __RESULT_SUCCESS)
		goto failure;
	state++;
	show_acks = 1;
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
end_tests(int index)
{
	show_acks = 0;
	if (stream_stop(2) != __RESULT_SUCCESS)
		goto failure;
	state++;
	if (stream_stop(1) != __RESULT_SUCCESS)
		goto failure;
	state++;
	if (stream_stop(0) != __RESULT_SUCCESS)
		goto failure;
	state++;
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

/*
 *  -------------------------------------------------------------------------
 *
 *  Injected event encoding and display functions.
 *
 *  -------------------------------------------------------------------------
 */

union primitives {
	lmi_ulong prim;
	union LMI_primitives lmi;
	union SDL_primitives sdl;
	union SDT_primitives sdt;
	union SL_primitives sl;
};

static int
do_signal(int child, int action)
{
	struct strbuf ctrl_buf, data_buf, *ctrl = &ctrl_buf, *data = &data_buf;
	char cbuf[BUFSIZE], dbuf[BUFSIZE];
	union primitives *p = (typeof(p)) cbuf;
	struct strioctl ic;
	int err;

	if (action != oldact) {
		oldact = action;
		show = 1;
		if (verbose > 1) {
			if (cntact > 0) {
				char buf[64];

				snprintf(buf, sizeof(buf), "Ct=%5d", cntact + 1);
				print_command_state(child, buf);
			}
		}
		cntact = 0;
	} else if (!show)
		cntact++;

	ic.ic_cmd = 0;
	ic.ic_timout = test_timout;
	ic.ic_len = sizeof(cbuf);
	ic.ic_dp = cbuf;
	ctrl->maxlen = 0;
	ctrl->buf = cbuf;
	data->maxlen = 0;
	data->buf = dbuf;
	test_pflags = MSG_BAND;
	test_pband = 0;
	switch (action) {
	case __TEST_SIO:
		if (!cntmsg)
			print_tx_msg(child, "SIO");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIO;
		data->len = 4;
		goto send_message;
	case __TEST_SIN:
		if (!cntmsg)
			print_tx_msg(child, "SIN");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIN;
		data->len = 4;
		goto send_message;
	case __TEST_SIE:
		if (!cntmsg)
			print_tx_msg(child, "SIE");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIE;
		data->len = 4;
		goto send_message;
	case __TEST_SIOS:
		if (!cntmsg)
			print_tx_msg(child, "SIOS");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIOS;
		data->len = 4;
		goto send_message;
	case __TEST_SIPO:
		if (!cntmsg)
			print_tx_msg(child, "SIPO");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIPO;
		data->len = 4;
		goto send_message;
	case __TEST_SIB:
		if (!cntmsg)
			print_tx_msg(child, "SIB");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIB;
		data->len = 4;
		goto send_message;
	case __TEST_SIX:
		if (!cntmsg)
			print_tx_msg(child, "LSSU(invalid)");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = 6;
		data->len = 4;
		goto send_message;
	case __TEST_SIO2:
		if (!cntmsg)
			print_tx_msg(child, "SIO[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = LSSU_SIO;
		data->len = 5;
		goto send_message;
	case __TEST_SIN2:
		if (!cntmsg)
			print_tx_msg(child, "SIN[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = LSSU_SIN;
		data->len = 5;
		goto send_message;
	case __TEST_SIE2:
		if (!cntmsg)
			print_tx_msg(child, "SIE[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = LSSU_SIE;
		data->len = 5;
		goto send_message;
	case __TEST_SIOS2:
		if (!cntmsg)
			print_tx_msg(child, "SIOS[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = LSSU_SIOS;
		data->len = 5;
		goto send_message;
	case __TEST_SIPO2:
		if (!cntmsg)
			print_tx_msg(child, "SIPO[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = LSSU_SIPO;
		data->len = 5;
		goto send_message;
	case __TEST_SIB2:
		if (!cntmsg)
			print_tx_msg(child, "SIB[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = LSSU_SIB;
		data->len = 5;
		goto send_message;
	case __TEST_SIX2:
		if (!cntmsg)
			print_tx_msg(child, "LSSU(invalid)[2]");
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 2;
		dbuf[3] = 0;
		dbuf[4] = 6;
		data->len = 5;
		goto send_message;
	case __TEST_SIB_S:
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 1;
		dbuf[3] = LSSU_SIB;
		data->len = 4;
		goto send_message;
	case __TEST_FISU:
		if (!cntmsg)
			print_tx_msg(child, "FISU");
	case __TEST_FISU_S:
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 0;
		data->len = 3;
		goto send_message;
	case __TEST_FISU_BAD_FIB:
		if (!cntmsg)
			print_tx_msg_sn(child, "FISU(bad fib)", bib[child] | bsn[child], (fib[child] | fsn[child]) ^ 0x80);
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = (fib[child] | fsn[child]) ^ 0x80;
		dbuf[2] = 0;
		data->len = 3;
		goto send_message;
	case __TEST_LSSU_CORRUPT:
		if (!cntmsg)
			print_tx_msg(child, "LSSU(corrupt)");
	case __TEST_LSSU_CORRUPT_S:
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 0xff;
		dbuf[3] = 0xff;
		data->len = 4;
		goto send_message;
	case __TEST_FISU_CORRUPT:
		if (!cntmsg)
			print_tx_msg(child, "FISU(corrupt)");
	case __TEST_FISU_CORRUPT_S:
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 0xff;
		data->len = 3;
		goto send_message;
	case __TEST_MSU:
		if (msu_len > BUFSIZE - 10)
			msu_len = BUFSIZE - 10;
		fsn[child] = (fsn[child] + 1) & 0x7f;
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = msu_len;
		memset(&dbuf[3], 'B', msu_len);
		data->len = msu_len + 3;
		if (!cntmsg)
			print_tx_msg(child, "MSU");
		goto send_message;
	case __TEST_MSU_S:
		if (msu_len > BUFSIZE - 10)
			msu_len = BUFSIZE - 10;
		fsn[child] = (fsn[child] + 1) & 0x7f;
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = msu_len;
		memset(&dbuf[3], 'B', msu_len);
		data->len = msu_len + 3;
		goto send_message;
	case __TEST_MSU_TOO_LONG:
		fsn[child] = (fsn[child] + 1) & 0x7f;
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		dbuf[2] = 63;
		memset(&dbuf[3], 'A', 280);
		data->len = 283;
		if (!cntmsg)
			print_tx_msg(child, "MSU(too long)");
		goto send_message;
	case __TEST_MSU_SEVEN_ONES:
		msu_len = 256;
		fsn[child] = (fsn[child] + 1) & 0x7f;
		if (!cntmsg)
			print_tx_msg(child, "MSU(7 ones)");
		fsn[child] = (fsn[child] - 1) & 0x7f;
		{
			struct strioctl ctl;

			ctl.ic_cmd = SDT_IOCCABORT;
			ctl.ic_timout = 0;
			ctl.ic_len = 0;
			ctl.ic_dp = NULL;
			do_signal(child, __TEST_MSU_S);
			if (ioctl(test_fd[child], I_STR, &ctl) < 0)
				return __RESULT_INCONCLUSIVE;
		}
		return __RESULT_SUCCESS;
	case __TEST_MSU_TOO_SHORT:
		fsn[child] = (fsn[child] + 1) & 0x7f;
		dbuf[0] = bib[child] | bsn[child];
		dbuf[1] = fib[child] | fsn[child];
		data->len = 2;
		if (!cntmsg)
			print_tx_msg(child, "MSU(too short)");
		goto send_message;
	case __TEST_FISU_FISU_1FLAG:
		print_tx_msg(child, "FISU-F-FISU");
		do_signal(child, __TEST_FISU_S);
		do_signal(child, __TEST_FISU_S);
		return __RESULT_SUCCESS;
	case __TEST_FISU_FISU_2FLAG:
		print_tx_msg(child, "FISU-F-F-FISU");
		config->sdt.f = SDT_FLAGS_TWO;
		if (do_signal(child, __TEST_SDT_CONFIG) != __RESULT_SUCCESS)
			return __RESULT_INCONCLUSIVE;
		do_signal(child, __TEST_FISU_S);
		do_signal(child, __TEST_FISU_S);
		config->sdt.f = SDT_FLAGS_ONE;
		if (do_signal(child, __TEST_SDT_CONFIG) != __RESULT_SUCCESS)
			return __RESULT_INCONCLUSIVE;
		return __RESULT_SUCCESS;
	case __TEST_MSU_MSU_1FLAG:
		print_tx_msg(child, "MSU-F-MSU");
		do_signal(child, __TEST_MSU_S);
		do_signal(child, __TEST_MSU_S);
		return __RESULT_SUCCESS;
	case __TEST_MSU_MSU_2FLAG:
		print_tx_msg(child, "MSU-F-F-MSU");
		config->sdt.f = SDT_FLAGS_TWO;
		if (do_signal(child, __TEST_SDT_CONFIG) != __RESULT_SUCCESS)
			return __RESULT_INCONCLUSIVE;
		do_signal(child, __TEST_MSU_S);
		do_signal(child, __TEST_MSU_S);
		config->sdt.f = SDT_FLAGS_ONE;
		if (do_signal(child, __TEST_SDT_CONFIG) != __RESULT_SUCCESS)
			return __RESULT_INCONCLUSIVE;
		return __RESULT_SUCCESS;
	      send_message:
		data->maxlen = BUFSIZE;
		data->len = data->len;
		data->buf = dbuf;
		ctrl->maxlen = BUFSIZE;
		ctrl->len = sizeof(p->sdt.daedt_transmission_req);
		ctrl->buf = cbuf;
		p->sdt.daedt_transmission_req.sdt_primitive = SDT_DAEDT_TRANSMISSION_REQ;
		return test_putmsg(child, ctrl, data, 0);
	case __TEST_WRITE:
		data->len = snprintf(dbuf, BUFSIZE, "%s", "Write test data.");
		return test_write(child, dbuf, data->len);
	case __TEST_WRITEV:
	{
		struct iovec vector[4];

		vector[0].iov_base = dbuf;
		vector[0].iov_len = sprintf(vector[0].iov_base, "Writev test datum for vector 0.");
		vector[1].iov_base = dbuf + vector[0].iov_len;
		vector[1].iov_len = sprintf(vector[1].iov_base, "Writev test datum for vector 1.");
		vector[2].iov_base = dbuf + vector[1].iov_len;
		vector[2].iov_len = sprintf(vector[2].iov_base, "Writev test datum for vector 2.");
		vector[3].iov_base = dbuf + vector[2].iov_len;
		vector[3].iov_len = sprintf(vector[3].iov_base, "Writev test datum for vector 3.");
		return test_writev(child, vector, 4);
	}
	}
	switch (action) {
	case __TEST_PUSH:
		return test_push(child, "m2pa-sl");
	case __TEST_POP:
		return test_pop(child);
	case __TEST_PUTMSG_DATA:
		ctrl = NULL;
		data->len = snprintf(dbuf, BUFSIZE, "%s", "Putmsg test data.");
		test_pflags = MSG_BAND;
		test_pband = 0;
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_PUTPMSG_DATA:
		ctrl = NULL;
		data->len = snprintf(dbuf, BUFSIZE, "%s", "Putpmsg band test data.");
		test_pflags = MSG_BAND;
		test_pband = 1;
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);

/*
 *  The following group cannot really be performed on the PT (child == 1) and
 *  they are only used to print the messages that would appear if the PT were
 *  functioning similar to the IUT.
 */

	case __TEST_POWER_ON:
		if (child == CHILD_PTU) {
			ctrl->len = sizeof(p->sdt.daedt_start_req);
			p->sdt.daedt_start_req.sdt_primitive = SDT_DAEDT_START_REQ;
			data = NULL;
			test_putpmsg(child, ctrl, data, test_pband, test_pflags);
			ctrl->len = sizeof(p->sdt.daedr_start_req);
			p->sdt.daedr_start_req.sdt_primitive = SDT_DAEDR_START_REQ;
			data = NULL;
		} else {
			ctrl->len = sizeof(p->sl.power_on_req);
			p->sl.power_on_req.sl_primitive = SL_POWER_ON_REQ;
			data = NULL;
			test_pflags = MSG_HIPRI;
			test_pband = 0;
		}
		print_command_state(child, ":power on");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_START:
		ctrl->len = sizeof(p->sl.start_req);
		p->sl.start_req.sl_primitive = SL_START_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":start");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_STOP:
		ctrl->len = sizeof(p->sl.stop_req);
		p->sl.stop_req.sl_primitive = SL_STOP_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":stop");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_LPO:
		ctrl->len = sizeof(p->sl.local_proc_outage_req);
		p->sl.local_proc_outage_req.sl_primitive = SL_LOCAL_PROCESSOR_OUTAGE_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":set lpo");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_LPR:
		ctrl->len = sizeof(p->sl.resume_req);
		p->sl.resume_req.sl_primitive = SL_RESUME_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":clear lpo");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_CONTINUE:
		if (((ss7_pvar & SS7_PVAR_MASK) != SS7_PVAR_ANSI) || ((ss7_pvar & SS7_PVAR_YR) == SS7_PVAR_88)) {
			ctrl->len = sizeof(p->sl.continue_req);
			p->sl.continue_req.sl_primitive = SL_CONTINUE_REQ;
			data = NULL;
			test_pflags = MSG_HIPRI;
			test_pband = 0;
			print_command_state(child, ":continue");
			if (child != CHILD_PTU)
				return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		}
		return __RESULT_SUCCESS;
	case __TEST_CONG_A:
		ctrl->len = sizeof(p->sl.cong_accept_req);
		p->sl.cong_accept_req.sl_primitive = SL_CONGESTION_ACCEPT_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":make congested");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_CONG_D:
		ctrl->len = sizeof(p->sl.cong_discard_req);
		p->sl.cong_discard_req.sl_primitive = SL_CONGESTION_DISCARD_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":make congested");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_NO_CONG:
		ctrl->len = sizeof(p->sl.no_cong_req);
		p->sl.no_cong_req.sl_primitive = SL_NO_CONGESTION_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":clear congested");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_EMERG:
		ctrl->len = sizeof(p->sl.emergency_req);
		p->sl.emergency_req.sl_primitive = SL_EMERGENCY_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":set emerg");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_CEASE:
		ctrl->len = sizeof(p->sl.emergency_ceases_req);
		p->sl.emergency_ceases_req.sl_primitive = SL_EMERGENCY_CEASES_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":clear emerg");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;
	case __TEST_CLEARB:
		ctrl->len = sizeof(p->sl.clear_buffers_req);
		p->sl.clear_buffers_req.sl_primitive = SL_CLEAR_BUFFERS_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		print_command_state(child, ":clear buffers");
		if (child != CHILD_PTU)
			return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
		return __RESULT_SUCCESS;

	case __TEST_COUNT:
	{
		char buf[64];

		snprintf(buf, sizeof(buf), "Ct = %5d", count);
		print_string(child, buf);
		return __RESULT_SUCCESS;
	}
	case __TEST_TRIES:
	{
		char buf[64];

		snprintf(buf, sizeof(buf), "%d iterations", tries);
		print_string(child, buf);
		return __RESULT_SUCCESS;
	}
	case __TEST_ETC:
		print_string(child, ".");
		print_string(child, ".");
		print_string(child, ".");
		return __RESULT_SUCCESS;

	case __TEST_SEND_MSU:
	case __TEST_SEND_MSU_S:
		if (msu_len > BUFSIZE - 10)
			msu_len = BUFSIZE - 10;
		print_command_state(child, ":msu");
		ctrl->len = sizeof(p->sl.pdu_req);
		p->sl.pdu_req.sl_primitive = SL_PDU_REQ;
		data->len = msu_len;
		memset(dbuf, 'B', msu_len);
		test_pflags = MSG_BAND;
		test_pband = 0;
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_TX_BREAK:
		print_command_state(child, ":break Tx");
		ic.ic_cmd = SDL_IOCCDISCTX;
		ic.ic_timout = 0;
		ic.ic_len = 0;
		ic.ic_dp = NULL;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_TX_MAKE:
		print_command_state(child, ":reconnect Tx");
		ic.ic_cmd = SDL_IOCCCONNTX;
		ic.ic_timout = 0;
		ic.ic_len = 0;
		ic.ic_dp = NULL;
		return test_ioctl(child, I_STR, (intptr_t) &ic);

	case __TEST_SDL_OPTIONS:
		if (show && verbose > 1)
			print_command_state(child, ":options sdl");
		ic.ic_cmd = SDL_IOCSOPTIONS;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(config->opt);
		ic.ic_dp = (char *) &config->opt;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_SDL_CONFIG:
		if (show && verbose > 1)
			print_command_state(child, ":config sdl");
		ic.ic_cmd = SDL_IOCSCONFIG;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(config->sdl);
		ic.ic_dp = (char *) &config->sdl;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_SDL_STATS:
		if (show && verbose > 1)
			print_command_state(child, ":stats sdl");
		ic.ic_cmd = SDL_IOCGSTATS;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(stats->sdl);
		ic.ic_dp = (char *) &stats->sdl;
		if (!(err = test_ioctl(child, I_STR, (intptr_t) &ic)))
			if (show && verbose > 1)
				print_sdl_stats(child, &stats->sdl);
		return (err);
	case __TEST_SDT_OPTIONS:
		if (show && verbose > 1)
			print_command_state(child, ":options sdt");
		ic.ic_cmd = SDT_IOCSOPTIONS;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(config->opt);
		ic.ic_dp = (char *) &config->opt;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_SDT_CONFIG:
		if (show && verbose > 1)
			print_command_state(child, ":config sdt");
		ic.ic_cmd = SDT_IOCSCONFIG;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(config->sdt);
		ic.ic_dp = (char *) &config->sdt;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_SDT_STATS:
		if (show && verbose > 1)
			print_command_state(child, ":stats sdt");
		ic.ic_cmd = SDT_IOCGSTATS;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(stats->sdt);
		ic.ic_dp = (char *) &stats->sdt;
		if (!(err = test_ioctl(child, I_STR, (intptr_t) &ic)))
			if (show && verbose > 1)
				print_sdt_stats(child, &stats->sdt);
		return (err);
	case __TEST_SL_OPTIONS:
		if (show && verbose > 1)
			print_command_state(child, ":options sl");
		ic.ic_cmd = SL_IOCSOPTIONS;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(config->opt);
		ic.ic_dp = (char *) &config->opt;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_SL_CONFIG:
		if (show && verbose > 1)
			print_command_state(child, ":config sl");
		ic.ic_cmd = SL_IOCSCONFIG;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(config->sl);
		ic.ic_dp = (char *) &config->sl;
		return test_ioctl(child, I_STR, (intptr_t) &ic);
	case __TEST_SL_STATS:
		if (show && verbose > 1)
			print_command_state(child, ":stats sl");
		ic.ic_cmd = SL_IOCGSTATS;
		ic.ic_timout = 0;
		ic.ic_len = sizeof(stats->sl);
		ic.ic_dp = (char *) &stats->sl;
		if (!(err = test_ioctl(child, I_STR, (intptr_t) &ic)))
			if (show && verbose > 1)
				print_sl_stats(child, &stats->sl);
		return (err);

	case __TEST_ATTACH_REQ:
		ctrl->len = sizeof(p->lmi.attach_req) + (ADDR_buffer ? ADDR_length : 0);
		p->lmi.attach_req.lmi_primitive = LMI_ATTACH_REQ;
		p->lmi.attach_req.lmi_ppa_length = ADDR_length;
		p->lmi.attach_req.lmi_ppa_offset = sizeof(p->lmi.attach_req);
		if (ADDR_buffer)
			bcopy(ADDR_buffer, ctrl->buf + sizeof(p->lmi.attach_req), ADDR_length);
		data = NULL;
		test_pflags = MSG_BAND;
		test_pband = 0;
		if (show && verbose > 1) {
			print_command_state(child, ":attach");
			print_ppa(child, (ppa_t *) p->lmi.attach_req.lmi_ppa);
		}
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_DETACH_REQ:
		ctrl->len = sizeof(p->lmi.detach_req);
		p->lmi.detach_req.lmi_primitive = LMI_DETACH_REQ;
		data = NULL;
		test_pflags = MSG_BAND;
		test_pband = 0;
		if (show && verbose > 1)
			print_command_state(child, ":detach");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_ENABLE_REQ:
		ctrl->len = sizeof(p->lmi.enable_req) + (ADDR_buffer ? ADDR_length : 0);
		p->lmi.enable_req.lmi_primitive = LMI_ENABLE_REQ;
		p->lmi.enable_req.lmi_rem_length = ADDR_length;
		p->lmi.enable_req.lmi_rem_offset = sizeof(p->lmi.enable_req);
		if (ADDR_buffer)
			bcopy(ADDR_buffer, ctrl->buf + sizeof(p->lmi.enable_req), ADDR_length);
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		if (show && verbose > 1) {
			print_command_state(child, ":enable");
			print_ppa(child, (ppa_t *) p->lmi.enable_req.lmi_rem);
		}
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_DISABLE_REQ:
		ctrl->len = sizeof(p->lmi.disable_req);
		p->lmi.disable_req.lmi_primitive = LMI_DISABLE_REQ;
		data = NULL;
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		if (show && verbose > 1)
			print_command_state(child, ":disable");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);

#if 0
	case __TEST_BIND_REQ:
		ctrl->len = sizeof(p->npi.bind_req) + anums[child] * sizeof(addrs[child][0]);
		data = NULL;
		p->npi.bind_req.PRIM_type = N_BIND_REQ;
		p->npi.bind_req.ADDR_length = anums[child] * sizeof(addrs[child][0]);
		p->npi.bind_req.ADDR_offset = sizeof(p->npi.bind_req);
		p->npi.bind_req.CONIND_number = (child == 2) ? 2 : 0;
		p->npi.bind_req.BIND_flags = TOKEN_REQUEST;
		p->npi.bind_req.PROTOID_length = 0;
		p->npi.bind_req.PROTOID_offset = 0;
		bcopy(&addrs[child], (&p->npi.bind_req + 1), anums[child] * sizeof(addrs[child][0]));
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		if (show && verbose > 1)
			print_command_state(child, ">bind");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_CONN_REQ:
		ctrl->len = sizeof(p->npi.conn_req) + sizeof(qos[child].conn);
		data = NULL;
		p->npi.conn_req.PRIM_type = N_CONN_REQ;
		p->npi.conn_req.DEST_length = anums[(child + 1) % 2] * sizeof(addrs[child][0]);
		p->npi.conn_req.DEST_offset = sizeof(p->npi.conn_req);
		p->npi.conn_req.CONN_flags = REC_CONF_OPT | EX_DATA_OPT;
		p->npi.conn_req.QOS_length = sizeof(qos[child].conn);
		p->npi.conn_req.QOS_offset = sizeof(p->npi.conn_req) + anums[(child + 1) % 2] * sizeof(addrs[child][0]);
		bcopy(&addrs[(child + 1) % 2], (&p->npi.conn_req + 1), anums[(child + 1) % 2] * sizeof(addrs[child][0]));
		bcopy(&qos[child].conn, (caddr_t) (&p->npi.conn_req + 1) + anums[(child + 1) % 2] * sizeof(addrs[child][0]), sizeof(qos[child].conn));
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		if (show && verbose > 1)
			print_command_state(child, ">connect");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_CONN_RES:
		ctrl->len = sizeof(p->npi.conn_res) + sizeof(qos[child].conn);
		data = NULL;
		p->npi.conn_res.PRIM_type = N_CONN_RES;
		p->npi.conn_res.RES_length = 0;
		p->npi.conn_res.RES_offset = 0;
		p->npi.conn_res.SEQ_number = seq[child];
		p->npi.conn_res.CONN_flags = REC_CONF_OPT | EX_DATA_OPT;
		p->npi.conn_res.QOS_length = sizeof(qos[child].conn);
		p->npi.conn_res.QOS_offset = sizeof(p->npi.conn_res);
		bcopy(&qos[child].conn, (&p->npi.conn_res + 1), sizeof(qos[child].conn));
		test_pflags = MSG_HIPRI;
		test_pband = 0;
		if (show && verbose > 1)
			print_command_state(child, ">accept");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_DISCON_REQ:
		ctrl->len = sizeof(p->npi.discon_req);
		data = NULL;
		p->npi.discon_req.PRIM_type = N_DISCON_REQ;
		p->npi.discon_req.RES_length = 0;
		p->npi.discon_req.RES_offset = 0;
		p->npi.discon_req.SEQ_number = 0;
		test_pflags = MSG_BAND;
		test_pband = 0;
		if (show && verbose > 1)
			print_command_state(child, ">disconnect");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
	case __TEST_OPTMGMT_REQ:
		ctrl->len = sizeof(p->npi.optmgmt_req) + sizeof(qos[child].info);
		p->npi.optmgmt_req.QOS_length = sizeof(qos[child].info);
		p->npi.optmgmt_req.QOS_offset = sizeof(p->npi.optmgmt_req);
		p->npi.optmgmt_req.OPTMGMT_flags = 0;
		bcopy(&qos[child].info, (&p->npi.optmgmt_req + 1), sizeof(qos[child].info));
		if (show && verbose > 1)
			print_command_state(child, ">optmgmt");
		return test_putpmsg(child, ctrl, data, test_pband, test_pflags);
#endif

	default:
		if (show && verbose > 1)
			print_command_state(child, ":????????");
		return __RESULT_SCRIPT_ERROR;
	}
	return __RESULT_SCRIPT_ERROR;
}

/*
 *  -------------------------------------------------------------------------
 *
 *  Received event decoding and display functions.
 *
 *  -------------------------------------------------------------------------
 */

static int
do_decode_data(int child, struct strbuf *ctrl, struct strbuf *data)
{
	int event = __RESULT_DECODE_ERROR;
	int other = (child + 1) % 2;

	if (data->len >= 0) {
		switch (child) {
		default:
			event = __TEST_DATA;
			print_rx_data(child, "M_DATA----------", data->len);
			break;
		case 0:
		{
			bib[other] = data->buf[0] & 0x80;
			bsn[other] = data->buf[0] & 0x7f;
			fib[other] = data->buf[1] & 0x80;
			fsn[other] = data->buf[1] & 0x7f;
			li[other] = data->buf[2] & 0x3f;
			sio[other] = data->buf[3] & 0x7;
			bsn[child] = fsn[other];
			switch (li[other]) {
			case 0:
				event = __TEST_FISU;
				break;
			case 1:
				event = sio[other] = data->buf[3] & 0x7;
				switch (sio[other]) {
				case LSSU_SIO:
					event = __TEST_SIO;
					break;
				case LSSU_SIN:
					event = __TEST_SIN;
					break;
				case LSSU_SIE:
					event = __TEST_SIE;
					break;
				case LSSU_SIOS:
					event = __TEST_SIOS;
					break;
				case LSSU_SIPO:
					event = __TEST_SIPO;
					break;
				case LSSU_SIB:
					event = __TEST_SIB;
					break;
				default:
					event = __TEST_SIX;
					break;
				}
				break;
			case 2:
				event = sio[other] = data->buf[4] & 0x7;
				switch (sio[other]) {
				case LSSU_SIO:
					event = __TEST_SIO2;
					break;
				case LSSU_SIN:
					event = __TEST_SIN2;
					break;
				case LSSU_SIE:
					event = __TEST_SIE2;
					break;
				case LSSU_SIOS:
					event = __TEST_SIOS2;
					break;
				case LSSU_SIPO:
					event = __TEST_SIPO2;
					break;
				case LSSU_SIB:
					event = __TEST_SIB2;
					break;
				default:
					event = __TEST_SIX2;
					break;
				}
				break;
			default:
				event = __TEST_MSU;
				break;
			}
			if (show_fisus || event != __TEST_FISU) {
				if (event != oldret || oldisb != (((bib[other] | bsn[other]) << 8) | (fib[other] | fsn[other]))) {
					// if (oldisb == (((bib[other] | bsn[other]) << 8) |
					// (fib[other] |
					// fsn[other])) &&
					// ((event == __TEST_FISU && oldret == __TEST_MSU) ||
					// (event == __TEST_MSU && oldret == __TEST_FISU))) {
					// if (event == __TEST_MSU && !expand)
					// cntmsg++;
					// } else
					// cntret = 0;
					oldret = event;
					oldisb = ((bib[other] | bsn[other]) << 8) | (fib[other] | fsn[other]);
					// if (cntret) {
					// printf(" Ct=%d\n", cntret + 1);
					// fflush(stdout);
					// }
					cntret = 0;
				} else if (!expand)
					cntret++;
			}
			if (!cntret) {
				char buf[32];

				switch (event) {
				case __TEST_FISU:
					if (show_fisus) {
						snprintf(buf, sizeof(buf), "FISU");
						print_rx_msg(child, buf);
					}
					break;
				case __TEST_SIO:
					if (li[other] == 1)
						snprintf(buf, sizeof(buf), "SIO");
					else
						snprintf(buf, sizeof(buf), "SIO[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIN:
					if (li[other] == 1)
						snprintf(buf, sizeof(buf), "SIN");
					else
						snprintf(buf, sizeof(buf), "SIN[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIE:
					if (li[other] == 1)
						snprintf(buf, sizeof(buf), "SIE");
					else
						snprintf(buf, sizeof(buf), "SIE[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIOS:
					if (li[other] == 1)
						snprintf(buf, sizeof(buf), "SIOS");
					else
						snprintf(buf, sizeof(buf), "SIOS[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIPO:
					if (li[other] == 1)
						snprintf(buf, sizeof(buf), "SIPO");
					else
						snprintf(buf, sizeof(buf), "SIPO[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIB:
					if (li[other] == 1)
						snprintf(buf, sizeof(buf), "SIB");
					else
						snprintf(buf, sizeof(buf), "SIB[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIX:
					snprintf(buf, sizeof(buf), "LSSU(invalid)[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_SIX2:
					snprintf(buf, sizeof(buf), "LSSU(invalid)[%d]", li[other]);
					print_rx_msg(child, buf);
					break;
				case __TEST_MSU:
					if (show_msus) {
						snprintf(buf, sizeof(buf), "MSU[%d]", li[other]);
						print_rx_msg(child, buf);
					}
					break;
				}
			}
		}
		}
	}
	return ((last_event = event));
}

static int
do_decode_ctrl(int child, struct strbuf *ctrl, struct strbuf *data)
{
	int event = __RESULT_DECODE_ERROR;
	union primitives *p = (union primitives *) ctrl->buf;

	if (ctrl->len >= sizeof(p->prim)) {
		switch ((last_prim = p->prim)) {

		case SL_REMOTE_PROCESSOR_OUTAGE_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_IUT_RPO;
			break;
		case SL_REMOTE_PROCESSOR_RECOVERED_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_IUT_RPR;
			break;
		case SL_IN_SERVICE_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_IUT_IN_SERVICE;
			break;
		case SL_OUT_OF_SERVICE_IND:
			print_command_state(child, oos_string(p->sl.out_of_service_ind.sl_reason));
			event = __EVENT_IUT_OUT_OF_SERVICE;
			break;
		case SL_PDU_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_IUT_DATA;
			break;
		case SL_LINK_CONGESTED_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SL_LINK_CONGESTION_CEASED_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SL_RETRIEVED_MESSAGE_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SL_RETRIEVAL_COMPLETE_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SL_RB_CLEARED_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SL_BSNT_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SL_RTB_CLEARED_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;

		case SDT_RC_SIGNAL_UNIT_IND:
			if (verbose > 5 && show_msg)
				print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SDT_IAC_CORRECT_SU_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SDT_IAC_ABORT_PROVING_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SDT_LSC_LINK_FAILURE_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SDT_TXC_TRANSMISSION_REQUEST_IND:
			if (verbose > 5 && show_msg)
				print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case SDT_RC_CONGESTION_ACCEPT_IND:
		case SDT_RC_CONGESTION_DISCARD_IND:
		case SDT_RC_NO_CONGESTION_IND:
			print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;

		case LMI_INFO_ACK:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case LMI_OK_ACK:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __TEST_OK_ACK;
			break;
		case LMI_ERROR_ACK:
			if (show && verbose > 1) {
				print_command_state(child, prim_string(p->prim));
				print_string(child, lmerrno_string(p->lmi.error_ack.lmi_errno, p->lmi.error_ack.lmi_reason));
			}
			event = __TEST_ERROR_ACK;
			break;
		case LMI_ENABLE_CON:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __TEST_ENABLE_CON;
			break;
		case LMI_DISABLE_CON:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __TEST_DISABLE_CON;
			break;
		case LMI_ERROR_IND:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __TEST_ERROR_IND;
			break;
		case LMI_STATS_IND:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		case LMI_EVENT_IND:
			if (show && verbose > 1)
				print_command_state(child, prim_string(p->prim));
			event = __EVENT_UNKNOWN;
			break;
		default:
			event = __EVENT_UNKNOWN;
			print_no_prim(child, p->prim);
			break;
		}
		if (data && data->len >= 0)
			if ((last_event = do_decode_data(child, ctrl, data)) != __EVENT_UNKNOWN)
				event = last_event;
	}
	return ((last_event = event));
}

static int
do_decode_msg(int child, struct strbuf *ctrl, struct strbuf *data)
{
	if (ctrl->len > 0) {
		if ((last_event = do_decode_ctrl(child, ctrl, data)) != __EVENT_UNKNOWN)
			return time_event(child, last_event);
	} else if (data->len > 0) {
		if ((last_event = do_decode_data(child, ctrl, data)) != __EVENT_UNKNOWN)
			return time_event(child, last_event);
	}
	return ((last_event = __EVENT_NO_MSG));
}

int
wait_event(int child, int wait)
{
	while (1) {
		struct pollfd pfd[] = { {test_fd[child], POLLIN | POLLPRI, 0} };

		if (timer_timeout) {
			timer_timeout = 0;
			print_timeout(child);
			last_event = __EVENT_TIMEOUT;
			return time_event(child, __EVENT_TIMEOUT);
		}
		if (show && verbose > 4)
			print_syscall(child, "poll()");
		pfd[0].fd = test_fd[child];
		pfd[0].events = POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND | POLLMSG | POLLERR | POLLHUP;
		pfd[0].revents = 0;
		switch (poll(pfd, 1, wait)) {
		case -1:
			if (errno == EINTR || errno == ERESTART)
				continue;
			print_errno(child, (last_errno = errno));
			return (__RESULT_FAILURE);
		case 0:
			if (show && verbose > 4)
				print_success(child);
			print_nothing(child);
			last_event = __EVENT_NO_MSG;
			return time_event(child, __EVENT_NO_MSG);
		case 1:
			if (show && verbose > 4)
				print_success(child);
			if (pfd[0].revents) {
				int ret;

				ctrl.maxlen = BUFSIZE;
				ctrl.len = -1;
				ctrl.buf = cbuf;
				data.maxlen = BUFSIZE;
				data.len = -1;
				data.buf = dbuf;
				flags = 0;
				for (;;) {
					if ((verbose > 3 && show) || (verbose > 5 && show_msg))
						print_syscall(child, "getmsg()");
					if ((ret = getmsg(test_fd[child], &ctrl, &data, &flags)) >= 0)
						break;
					if (errno == EINTR || errno == ERESTART)
						continue;
					print_errno(child, (last_errno = errno));
					if (errno == EAGAIN)
						break;
					return __RESULT_FAILURE;
				}
				if (ret < 0)
					break;
				if (ret == 0) {
					if ((verbose > 3 && show) || (verbose > 5 && show_msg))
						print_success(child);
					if ((verbose > 3 && show) || (verbose > 5 && show_msg)) {
						int i;

						dummy = lockf(fileno(stdout), F_LOCK, 0);
						fprintf(stdout, "gotmsg from %d [%d,%d]:\n", child, ctrl.len, data.len);
						fprintf(stdout, "[");
						for (i = 0; i < ctrl.len; i++)
							fprintf(stdout, "%02X", (uint8_t) ctrl.buf[i]);
						fprintf(stdout, "]\n");
						fprintf(stdout, "[");
						for (i = 0; i < data.len; i++)
							fprintf(stdout, "%02X", (uint8_t) data.buf[i]);
						fprintf(stdout, "]\n");
						fflush(stdout);
						dummy = lockf(fileno(stdout), F_ULOCK, 0);
					}
					last_prio = (flags == RS_HIPRI);
					if ((last_event = do_decode_msg(child, &ctrl, &data)) != __EVENT_NO_MSG)
						return time_event(child, last_event);
				}
			}
		default:
			break;
		}
	}
	return __EVENT_UNKNOWN;
}

int
get_event(int child)
{
	return wait_event(child, -1);
}

int
get_data(int child, int action)
{
	int ret = 0;

	switch (action) {
	case __TEST_READ:
	{
		char buf[BUFSIZE];

		test_read(child, buf, BUFSIZE);
		ret = last_retval;
		break;
	}
	case __TEST_READV:
	{
		char buf[BUFSIZE];
		static const size_t count = 4;
		struct iovec vector[] = {
			{buf, (BUFSIZE >> 2)},
			{buf + (BUFSIZE >> 2), (BUFSIZE >> 2)},
			{buf + (BUFSIZE >> 2) + (BUFSIZE >> 2), (BUFSIZE >> 2)},
			{buf + (BUFSIZE >> 2) + (BUFSIZE >> 2) + (BUFSIZE >> 2), (BUFSIZE >> 2)}
		};

		test_readv(child, vector, count);
		ret = last_retval;
		break;
	}
	case __TEST_GETMSG:
	{
		char buf[BUFSIZE];
		struct strbuf data = { BUFSIZE, 0, buf };

		if (test_getmsg(child, NULL, &data, &test_gflags) == __RESULT_FAILURE) {
			ret = last_retval;
			break;
		}
		ret = data.len;
		break;
	}
	case __TEST_GETPMSG:
	{
		char buf[BUFSIZE];
		struct strbuf data = { BUFSIZE, 0, buf };

		if (test_getpmsg(child, NULL, &data, &test_gband, &test_gflags) == __RESULT_FAILURE) {
			ret = last_retval;
			break;
		}
		ret = data.len;
		break;
	}
	}
	return (ret);
}

int
expect(int child, int wait, int want)
{
	if ((last_event = wait_event(child, wait)) == want)
		return (__RESULT_SUCCESS);
	print_expect(child, want);
	return (__RESULT_FAILURE);
}

/*
 *  -------------------------------------------------------------------------
 *
 *  Preambles and postambles
 *
 *  -------------------------------------------------------------------------
 */
static int
preamble_unbound(int child)
{
	return __RESULT_SUCCESS;
}

static int
postamble_unbound(int child)
{
	return __RESULT_SUCCESS;
}

static int
preamble_push(int child)
{
	return __RESULT_SUCCESS;
}

static int
postamble_pop(int child)
{
	return __RESULT_SUCCESS;
}

static int
preamble_attach(int child)
{
	int failed = 0;

	{
		ADDR_buffer = addrs[child];
		ADDR_length = anums[child] * sizeof(addrs[child][0]);
		if (do_signal(child, __TEST_ATTACH_REQ))
			failed = failed ? : state;
		state++;
		if (expect(child, NORMAL_WAIT, __TEST_OK_ACK))
			failed = failed ? : state;
		if (failed) {
			state = failed;
			goto failure;
		}
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
preamble_config(int child)
{
	if (preamble_attach(child))
		goto failure;
	if (child == CHILD_IUT) {
		if (do_signal(child, __TEST_SL_OPTIONS))
			goto failure;
		state++;
		if (do_signal(child, __TEST_SL_CONFIG))
			goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
postamble_detach(int child)
{
	int failed = 0;

	{
		state++;
		if (do_signal(child, __TEST_DETACH_REQ))
			failed = failed ? : state;
		state++;
		if (expect(child, NORMAL_WAIT, __TEST_OK_ACK))
			failed = failed ? : state;
	}
	if (failed) {
		state = failed;
		goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
postamble_stats(int child)
{
	int failed = 0;
	
	if (child != CHILD_PTU) {
		state++;
		if (do_signal(child, __TEST_SL_STATS))
			failed = failed ? : state;
	}
	state++;
	if (failed) {
		state = failed;
		goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
preamble_enable(int child)
{
	if (preamble_config(child))
		goto failure;
	state++;
	{
		int other = (child + 1) % 2;

		ADDR_buffer = addrs[other];
		ADDR_length = anums[other] * sizeof(addrs[other][0]);
		if (do_signal(child, __TEST_ENABLE_REQ))
			goto failure;
		state++;
		if (expect(child, INFINITE_WAIT, __TEST_ENABLE_CON))
			goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
postamble_disable(int child)
{
	int failed = 0;

	state++;
	{
		if (do_signal(child, __TEST_DISABLE_REQ))
			failed = failed ? : state;
		state++;
		if (expect(child, NORMAL_WAIT, __TEST_DISABLE_CON))
			failed = failed ? : state;
		state++;
		if (test_ioctl(child, I_FLUSH, FLUSHRW))
			failed = failed ? : state;
	}
	if (postamble_stats(child))
		failed = failed ? : state;
	if (postamble_detach(child))
		failed = failed ? : state;
	if (failed) {
		state = failed;
		goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
postamble_link_power_off(int child)
{
	int failed = 0;

	if (stop_tt())
		failed = failed ? : state;
	state++;
	if (expect(child, NORMAL_WAIT, __EVENT_NO_MSG))
		failed = failed ? : state;
	state++;
	test_msleep(child, LONG_WAIT);
	state++;
	if (postamble_disable(child))
		failed = failed ? : state;
	if (failed) {
		state = failed;
		goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

static int
preamble_none(int child)
{
	switch (child) {
	case CHILD_PTU:
		break;
	case CHILD_IUT:
		break;
	}
	return __RESULT_SUCCESS;
}

static int
postamble_none(int child)
{
	switch (child) {
	case CHILD_PTU:
		break;
	case CHILD_IUT:
		break;
	}
	return __RESULT_SUCCESS;
}

static int
preamble_link_power_off(int child)
{
	show_timeout = 0;

	bib[0] = fib[0] = bib[1] = fib[1] = 0x80;
	bsn[0] = fsn[0] = bsn[1] = fsn[1] = 0x7f;

	switch (child) {
	case CHILD_PTU:
		if (preamble_enable(child))
			goto failure;
		break;
	case CHILD_IUT:
		if (preamble_enable(child))
			goto failure;
		break;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

#if 0
static int
preamble_link_power_off_pr(int child)
{
	config->opt.popt = 0;
	return preamble_link_power_off(child);
}
static int
preamble_link_power_off_np(int child)
{
	config->opt.popt = SS7_POPT_NOPR;
	return preamble_link_power_off(child);
}
#endif
static int
preamble_link_power_on(int child)
{
	switch (child) {
	case CHILD_PTU:
		if (preamble_link_power_off(child))
			goto failure;
		state++;
		if (do_signal(child, __TEST_POWER_ON))
			goto failure;
		state++;
		test_msleep(child, LONG_WAIT);
		break;
	case CHILD_IUT:
		if (preamble_link_power_off(child))
			goto failure;
		state++;
		if (do_signal(child, __TEST_POWER_ON))
			goto failure;
		state++;
		test_msleep(child, LONG_WAIT);
		break;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

#if 0
static int
preamble_link_power_on_pr(int child)
{
	config->opt.popt = 0;
	return preamble_link_power_on(child);
}
#endif
static int
preamble_link_power_on_np(int child)
{
	config->opt.popt = SS7_POPT_NOPR;
	return preamble_link_power_on(child);
}
static int
preamble_link_out_of_service(int child)
{
	switch (child) {
	case CHILD_PTU:
		if (preamble_link_power_on(child))
			goto failure;
		state++;
		if (expect(child, INFINITE_WAIT, __TEST_SIOS))
			goto failure;
		state++;
		if (do_signal(child, __TEST_SIOS))
			goto failure;
		state++;
		test_msleep(child, LONG_WAIT);
		break;
	case CHILD_IUT:
		if (preamble_link_power_on(child))
			goto failure;
		state++;
		test_msleep(child, LONG_WAIT);
		break;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

#if 0
static int
preamble_link_out_of_service_pr(int child)
{
	config->opt.popt = 0;
	return preamble_link_out_of_service(child);
}
static int
preamble_link_out_of_service_np(int child)
{
	config->opt.popt = SS7_POPT_NOPR;
	return preamble_link_out_of_service(child);
}
#endif
static int
postamble_link_out_of_service(int child)
{
	int failed = 0;

	if (stop_tt())
		failed = failed ? : state;
	state++;
	if (child == CHILD_PTU) {
		if (expect(child, NORMAL_WAIT, __EVENT_NO_MSG))
			if (last_event != __TEST_SIOS)
				failed = failed ? : state;
		state++;
	} else {
		if (expect(child, NORMAL_WAIT, __EVENT_NO_MSG))
			if (last_event != __EVENT_IUT_OUT_OF_SERVICE)
				failed = failed ? : state;
		state++;
	}
	test_msleep(child, NORMAL_WAIT);
	state++;
	if (postamble_disable(child))
		failed = failed ? : state;
	if (failed) {
		state = failed;
		goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}
static int
preamble_link_in_service(int child)
{
	switch (child) {
	case CHILD_PTU:
		if (preamble_link_out_of_service(child))
			goto failure;
		state++;
		if (do_signal(child, __TEST_START))
			goto failure;
		state++;
		for (;;) {
			if (expect(child, INFINITE_WAIT, __TEST_SIO)) {
				if (last_event == __TEST_SIOS) {
					if (do_signal(child, __TEST_SIOS))
						goto failure;
					state++;
					continue;
				}
				goto failure;
			}
			if (do_signal(child, __TEST_SIO))
				goto failure;
			state++;
			break;
		}
		if (!(config->opt.popt & SS7_POPT_NOPR)) {
			for (;;) {
				if (expect(child, INFINITE_WAIT, __TEST_FISU)) {
					switch (last_event) {
					case __MSG_PROVING:
					case __TEST_SIE:
					case __TEST_SIN:
						if (do_signal(child, __TEST_SIE))
							goto failure;
						state++;
						print_less(child);
						continue;
					}
					goto failure;
				}
				print_more(child);
				if (do_signal(child, __TEST_FISU))
					goto failure;
				state++;
				break;
			}
		}
		state++;
		test_msleep(child, LONG_WAIT);
		break;
	case CHILD_IUT:
		if (preamble_link_out_of_service(child))
			goto failure;
		state++;
		if (do_signal(child, __TEST_START))
			goto failure;
		state++;
		if (expect(child, INFINITE_WAIT, __EVENT_IUT_IN_SERVICE))
			goto failure;
		state++;
		test_msleep(child, LONG_WAIT);
		break;
	}
	return __RESULT_SUCCESS;
      failure:
	print_more(child);
	return __RESULT_FAILURE;
}

#if 0
static int
preamble_link_in_service_pr(int child)
{
	config->opt.popt = 0;
	return preamble_link_in_service(child);
}
static int
preamble_link_in_service_np(int child)
{
	config->opt.popt = SS7_POPT_NOPR;
	return preamble_link_in_service(child);
}
static int
preamble_link_in_service_basic(int child)
{
	config->opt.popt = 0;
	return preamble_link_in_service(child);
}
#endif
static int
preamble_link_in_service_pcr(int child)
{
	config->opt.popt = SS7_POPT_PCR;
	return preamble_link_in_service(child);
}
static int
postamble_link_in_service(int child)
{
	int failed = 0;

	if (stop_tt())
		failed = failed ? : state;
	state++;
	if (child == CHILD_PTU) {
		if (do_signal(child, __TEST_SIOS))
			failed = failed ? : state;
		state++;
		for (;;) {
			if (expect(child, INFINITE_WAIT, __TEST_SIOS)) {
				switch (last_event) {
				case __TEST_FISU:
				case __TEST_SIPO:
				case __TEST_MSU:
					if (do_signal(child, __TEST_SIOS))
						failed = failed ? : state;
					state++;
					continue;
				}
			}
			break;
		}
		state++;
	} else {
		if (expect(child, INFINITE_WAIT, __EVENT_IUT_OUT_OF_SERVICE))
			failed = failed ? : state;
		state++;
	}
	if (postamble_link_out_of_service(child))
		failed = failed ? : state;
	if (failed) {
		state = failed;
		goto failure;
	}
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

/*
 *  =========================================================================
 *
 *  The test cases...
 *
 *  =========================================================================
 */

struct test_stream {
	int (*preamble) (int);		/* test preamble */
	int (*testcase) (int);		/* test case */
	int (*postamble) (int);		/* test postamble */
};

static int
check_snibs(int child, unsigned char bsnib, unsigned char fsnib)
{
	print_rx_msg_sn(child, "check b/f sn/ib", bsnib, fsnib);
	if ((bib[1] | bsn[1]) == bsnib && (fib[1] | fsn[1]) == fsnib)
		return __RESULT_SUCCESS;
	return __RESULT_FAILURE;
}

/*
 *  Check test case guard timer.
 */
#define test_group_0 "0. Sanity checks"
#define test_group_0_1 "0.1. Guard timers"
#define tgrp_case_0_1 test_group_0
#define sgrp_case_0_1 test_group_0_1
#define numb_case_0_1 "0.1"
#define name_case_0_1 "Check test case guard timer."
#define xtra_case_0_1 NULL
#define sref_case_0_1 "(none)"
#define desc_case_0_1 "\
Checks that the test case guard timer will fire and bring down the children."

int
test_0_1_ptu(int child)
{
	if (test_duration == INFINITE_WAIT)
		return __RESULT_NOTAPPL;
	test_msleep(child, test_duration + 1000);
	state++;
	return (__RESULT_SUCCESS);
}

int
test_0_1_iut(int child)
{
	if (test_duration == INFINITE_WAIT)
		return __RESULT_NOTAPPL;
	test_msleep(child, test_duration + 1000);
	state++;
	return (__RESULT_SUCCESS);
}

struct test_stream test_case_0_1_ptu = { preamble_none, &test_0_1_ptu, postamble_none };
struct test_stream test_case_0_1_iut = { preamble_none, &test_0_1_iut, postamble_none };

#define test_case_0_1 { &test_case_0_1_ptu, &test_case_0_1_iut, NULL }

#define test_group_0_2 "0.2. Preambles and Postambles"
#define tgrp_case_0_2_1 test_group_0
#define sgrp_case_0_2_1 test_group_0_2
#define numb_case_0_2_1 "0.2.1"
#define name_case_0_2_1 "Base preamble, base postamble."
#define xtra_case_0_2_1 NULL
#define sref_case_0_2_1 "(none)"
#define desc_case_0_2_1 "\
Checks that two Streams can be opened and closed."

int
test_0_2_1_ptu(int child)
{
	return __RESULT_SUCCESS;
}

int
test_0_2_1_iut(int child)
{
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_1_ptu = { preamble_none, test_0_2_1_ptu, postamble_none };
struct test_stream test_case_0_2_1_iut = { preamble_none, test_0_2_1_iut, postamble_none };

#define test_case_0_2_1 { &test_case_0_2_1_ptu, &test_case_0_2_1_iut, NULL }

#define tgrp_case_0_2_2 test_group_0
#define sgrp_case_0_2_2 test_group_0_2
#define numb_case_0_2_2 "0.2.2"
#define name_case_0_2_2 "Unbound preamble, unbound postamble."
#define xtra_case_0_2_2 NULL
#define sref_case_0_2_2 "(none)"
#define desc_case_0_2_2 "\
Checks that two Streams can be opened, information obtained, and closed."

int
test_0_2_2_ptu(int child)
{
	return __RESULT_SUCCESS;
}

int
test_0_2_2_iut(int child)
{
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_2_ptu = { preamble_unbound, test_0_2_2_ptu, postamble_unbound };
struct test_stream test_case_0_2_2_iut = { preamble_unbound, test_0_2_2_iut, postamble_unbound };

#define test_case_0_2_2 { &test_case_0_2_2_ptu, &test_case_0_2_2_iut, NULL }

#define tgrp_case_0_2_3 test_group_0
#define sgrp_case_0_2_3 test_group_0_2
#define numb_case_0_2_3 "0.2.3"
#define name_case_0_2_3 "Push preamble, pop postamble."
#define xtra_case_0_2_3 NULL
#define sref_case_0_2_3 "(none)"
#define desc_case_0_2_3 "\
Checks that two Streams can be opened, information obtained, module\n\
pushed, popped, and closed."

int
test_0_2_3_ptu(int child)
{
	return __RESULT_SUCCESS;
}

int
test_0_2_3_iut(int child)
{
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_3_ptu = { preamble_push, test_0_2_3_ptu, postamble_pop };
struct test_stream test_case_0_2_3_iut = { preamble_push, test_0_2_3_iut, postamble_pop };

#define test_case_0_2_3 { &test_case_0_2_3_ptu, &test_case_0_2_3_iut, NULL }

#define tgrp_case_0_2_4 test_group_0
#define sgrp_case_0_2_4 test_group_0_2
#define numb_case_0_2_4 "0.2.4"
#define name_case_0_2_4 "Config preamble, pop postamble."
#define xtra_case_0_2_4 NULL
#define sref_case_0_2_4 "(none)"
#define desc_case_0_2_4 "\
Checks that two Streams can be opened, information obtained, module\n\
pushed, configured, popped and closed."

int
test_0_2_4_ptu(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

int
test_0_2_4_iut(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_4_ptu = { preamble_config, test_0_2_4_ptu, postamble_pop };
struct test_stream test_case_0_2_4_iut = { preamble_config, test_0_2_4_iut, postamble_pop };

#define test_case_0_2_4 { &test_case_0_2_4_ptu, &test_case_0_2_4_iut, NULL }

#define tgrp_case_0_2_5 test_group_0
#define sgrp_case_0_2_5 test_group_0_2
#define numb_case_0_2_5 "0.2.5"
#define name_case_0_2_5 "Attach preamble, detach postamble."
#define xtra_case_0_2_5 NULL
#define sref_case_0_2_5 "(none)"
#define desc_case_0_2_5 "\
Checks that two Streams can be opened, information obtained, module\n\
pushed, configured, bound/attached, unbound/detached, module popped,\n\
and closed."

int
test_0_2_5_ptu(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

int
test_0_2_5_iut(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_5_ptu = { preamble_attach, test_0_2_5_ptu, postamble_detach };
struct test_stream test_case_0_2_5_iut = { preamble_attach, test_0_2_5_iut, postamble_detach };

#define test_case_0_2_5 { &test_case_0_2_5_ptu, &test_case_0_2_5_iut, NULL }

#define tgrp_case_0_2_6 test_group_0
#define sgrp_case_0_2_6 test_group_0_2
#define numb_case_0_2_6 "0.2.6"
#define name_case_0_2_6 "Enable preamble, disable postamble."
#define xtra_case_0_2_6 NULL
#define sref_case_0_2_6 "(none)"
#define desc_case_0_2_6 "\
Checks that two Streams can be opened, information obtained, module\n\
pushed, configured, bound/attached, connected/enabled, disconnected/\n\
disabled, unbound/detached, module popped and closed."

int
test_0_2_6_ptu(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

int
test_0_2_6_iut(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_6_ptu = { preamble_enable, test_0_2_6_ptu, postamble_disable };
struct test_stream test_case_0_2_6_iut = { preamble_enable, test_0_2_6_iut, postamble_disable };

#define test_case_0_2_6 { &test_case_0_2_6_ptu, &test_case_0_2_6_iut, NULL }

#define tgrp_case_0_2_7 test_group_0
#define sgrp_case_0_2_7 test_group_0_2
#define numb_case_0_2_7 "0.2.7"
#define name_case_0_2_7 "Link power on preamble, out of service postamble."
#define xtra_case_0_2_7 NULL
#define sref_case_0_2_7 "(none)"
#define desc_case_0_2_7 "\
Checks that two Streams can be opened, information obtained, module\n\
pushed, configured, bound/attached, connected/enabled, link power on,\n\
disconnected/disabled, unbound/detached, module popped and closed."

int
test_0_2_7_ptu(int child)
{
	if (expect(child, LONGEST_WAIT, __TEST_SIOS))
		goto failure;
	state++;
	if (do_signal(child, __TEST_SIOS))
		goto failure;
	state++;
	return __RESULT_SUCCESS;
      failure:
	return __RESULT_FAILURE;
}

int
test_0_2_7_iut(int child)
{
	test_sleep(child, 1);
	return __RESULT_SUCCESS;
}

struct test_stream test_case_0_2_7_ptu = { preamble_link_power_on, test_0_2_7_ptu, postamble_link_out_of_service };
struct test_stream test_case_0_2_7_iut = { preamble_link_power_on, test_0_2_7_iut, postamble_link_out_of_service };

#define test_case_0_2_7 { &test_case_0_2_7_ptu, &test_case_0_2_7_iut, NULL }


/*
 *  -------------------------------------------------------------------------
 *
 *  Test case child scheduler
 *
 *  -------------------------------------------------------------------------
 */
int
run_stream(int child, struct test_stream *stream)
{
	int result = __RESULT_SCRIPT_ERROR;
	int pre_result = __RESULT_SCRIPT_ERROR;
	int post_result = __RESULT_SCRIPT_ERROR;

	oldact = 0;		/* previous action for this child */
	cntact = 0;		/* count of this action for this child */
	oldevt = 0;		/* previous return for this child */
	cntevt = 0;		/* count of this return for this child */

	print_preamble(child);
	state = 100;
	failure_string = NULL;
	if (stream->preamble && (pre_result = stream->preamble(child)) != __RESULT_SUCCESS) {
		switch (pre_result) {
		case __RESULT_NOTAPPL:
			print_notapplicable(child);
			result = __RESULT_NOTAPPL;
			break;
		case __RESULT_SKIPPED:
			print_skipped(child);
			result = __RESULT_SKIPPED;
			break;
		default:
			print_inconclusive(child);
			result = __RESULT_INCONCLUSIVE;
			break;
		}
	} else {
		print_test(child);
		state = 200;
		failure_string = NULL;
		switch (stream->testcase(child)) {
		default:
		case __RESULT_INCONCLUSIVE:
			print_inconclusive(child);
			result = __RESULT_INCONCLUSIVE;
			break;
		case __RESULT_NOTAPPL:
			print_notapplicable(child);
			result = __RESULT_NOTAPPL;
			break;
		case __RESULT_SKIPPED:
			print_skipped(child);
			result = __RESULT_SKIPPED;
			break;
		case __RESULT_FAILURE:
			print_failed(child);
			result = __RESULT_FAILURE;
			break;
		case __RESULT_SCRIPT_ERROR:
			print_script_error(child);
			result = __RESULT_SCRIPT_ERROR;
			break;
		case __RESULT_SUCCESS:
			print_passed(child);
			result = __RESULT_SUCCESS;
			break;
		}
		print_postamble(child);
		state = 300;
		failure_string = NULL;
		if (stream->postamble && (post_result = stream->postamble(child)) != __RESULT_SUCCESS) {
			switch (post_result) {
			case __RESULT_NOTAPPL:
				print_notapplicable(child);
				result = __RESULT_NOTAPPL;
				break;
			case __RESULT_SKIPPED:
				print_skipped(child);
				result = __RESULT_SKIPPED;
				break;
			default:
				print_inconclusive(child);
				if (result == __RESULT_SUCCESS)
					result = __RESULT_INCONCLUSIVE;
				break;
			}
		}
	}
	print_test_end(child);
	exit(result);
}

/*
 *  Fork multiple children to do the actual testing.
 *  The conn child (child[0]) is the connecting process, the resp child
 *  (child[1]) is the responding process.
 */

int
test_run(struct test_stream *stream[], ulong duration)
{
	int children = 0;
	pid_t this_child, child[3] = { 0, };
	int this_status, status[3] = { 0, };

	if (start_tt(duration) != __RESULT_SUCCESS)
		goto inconclusive;
	if (server_exec && stream[2]) {
		switch ((child[2] = fork())) {
		case 00:	/* we are the child */
			exit(run_stream(2, stream[2]));	/* execute stream[2] state machine */
		case -1:	/* error */
			if (child[0])
				kill(child[0], SIGKILL);	/* toast stream[0] child */
			if (child[1])
				kill(child[1], SIGKILL);	/* toast stream[1] child */
			return __RESULT_FAILURE;
		default:	/* we are the parent */
			children++;
			break;
		}
	} else
		status[2] = __RESULT_SUCCESS;
	if (server_exec && stream[1]) {
		switch ((child[1] = fork())) {
		case 00:	/* we are the child */
			exit(run_stream(1, stream[1]));	/* execute stream[1] state machine */
		case -1:	/* error */
			if (child[0])
				kill(child[0], SIGKILL);	/* toast stream[0] child */
			return __RESULT_FAILURE;
		default:	/* we are the parent */
			children++;
			break;
		}
	} else
		status[1] = __RESULT_SUCCESS;
	if (client_exec && stream[0]) {
		switch ((child[0] = fork())) {
		case 00:	/* we are the child */
			exit(run_stream(0, stream[0]));	/* execute stream[0] state machine */
		case -1:	/* error */
			return __RESULT_FAILURE;
		default:	/* we are the parent */
			children++;
			break;
		}
	} else
		status[0] = __RESULT_SUCCESS;
	for (; children > 0; children--) {
	      waitagain:
		if ((this_child = wait(&this_status)) > 0) {
			if (WIFEXITED(this_status)) {
				if (this_child == child[0]) {
					child[0] = 0;
					if ((status[0] = WEXITSTATUS(this_status)) != __RESULT_SUCCESS) {
						if (child[1])
							kill(child[1], SIGKILL);
						if (child[2])
							kill(child[2], SIGKILL);
					}
				}
				if (this_child == child[1]) {
					child[1] = 0;
					if ((status[1] = WEXITSTATUS(this_status)) != __RESULT_SUCCESS) {
						if (child[0])
							kill(child[0], SIGKILL);
						if (child[2])
							kill(child[2], SIGKILL);
					}
				}
				if (this_child == child[2]) {
					child[2] = 0;
					if ((status[2] = WEXITSTATUS(this_status)) != __RESULT_SUCCESS) {
						if (child[0])
							kill(child[0], SIGKILL);
						if (child[1])
							kill(child[1], SIGKILL);
					}
				}
			} else if (WIFSIGNALED(this_status)) {
				int signal = WTERMSIG(this_status);

				if (this_child == child[0]) {
					print_terminated(0, signal);
					if (child[1])
						kill(child[1], SIGKILL);
					if (child[2])
						kill(child[2], SIGKILL);
					status[0] = (signal == SIGKILL) ? __RESULT_INCONCLUSIVE : __RESULT_FAILURE;
					child[0] = 0;
				}
				if (this_child == child[1]) {
					print_terminated(1, signal);
					if (child[0])
						kill(child[0], SIGKILL);
					if (child[2])
						kill(child[2], SIGKILL);
					status[1] = (signal == SIGKILL) ? __RESULT_INCONCLUSIVE : __RESULT_FAILURE;
					child[1] = 0;
				}
				if (this_child == child[2]) {
					print_terminated(2, signal);
					if (child[0])
						kill(child[0], SIGKILL);
					if (child[1])
						kill(child[1], SIGKILL);
					status[2] = (signal == SIGKILL) ? __RESULT_INCONCLUSIVE : __RESULT_FAILURE;
					child[2] = 0;
				}
			} else if (WIFSTOPPED(this_status)) {
				int signal = WSTOPSIG(this_status);

				if (this_child == child[0]) {
					print_stopped(0, signal);
					if (child[0])
						kill(child[0], SIGKILL);
					if (child[1])
						kill(child[1], SIGKILL);
					if (child[2])
						kill(child[2], SIGKILL);
					status[0] = __RESULT_FAILURE;
					child[0] = 0;
				}
				if (this_child == child[1]) {
					print_stopped(1, signal);
					if (child[0])
						kill(child[0], SIGKILL);
					if (child[1])
						kill(child[1], SIGKILL);
					if (child[2])
						kill(child[2], SIGKILL);
					status[1] = __RESULT_FAILURE;
					child[1] = 0;
				}
				if (this_child == child[2]) {
					print_stopped(2, signal);
					if (child[0])
						kill(child[0], SIGKILL);
					if (child[1])
						kill(child[1], SIGKILL);
					if (child[2])
						kill(child[2], SIGKILL);
					status[2] = __RESULT_FAILURE;
					child[2] = 0;
				}
			}
		} else {
			if (timer_timeout) {
				timer_timeout = 0;
				print_timeout(3);
			}
			if (child[0])
				kill(child[0], SIGKILL);
			if (child[1])
				kill(child[1], SIGKILL);
			if (child[2])
				kill(child[2], SIGKILL);
			goto waitagain;
		}
	}
	if (stop_tt() != __RESULT_SUCCESS)
		goto inconclusive;
	if (status[0] == __RESULT_NOTAPPL || status[1] == __RESULT_NOTAPPL || status[2] == __RESULT_NOTAPPL)
		return (__RESULT_NOTAPPL);
	if (status[0] == __RESULT_SKIPPED || status[1] == __RESULT_SKIPPED || status[2] == __RESULT_SKIPPED)
		return (__RESULT_SKIPPED);
	if (status[0] == __RESULT_FAILURE || status[1] == __RESULT_FAILURE || status[2] == __RESULT_FAILURE)
		return (__RESULT_FAILURE);
	if (status[0] == __RESULT_SUCCESS && status[1] == __RESULT_SUCCESS && status[2] == __RESULT_SUCCESS)
		return (__RESULT_SUCCESS);
      inconclusive:
	return (__RESULT_INCONCLUSIVE);
}

/*
 *  -------------------------------------------------------------------------
 *
 *  Test case lists
 *
 *  -------------------------------------------------------------------------
 */

struct test_case {
	const char *numb;		/* test case number */
	const char *tgrp;		/* test case group */
	const char *sgrp;		/* test case subgroup */
	const char *name;		/* test case name */
	const char *xtra;		/* test case extra information */
	const char *desc;		/* test case description */
	const char *sref;		/* test case standards section reference */
	struct test_stream *stream[3];	/* test streams */
	int (*start) (int);		/* start function */
	int (*stop) (int);		/* stop function */
	ulong duration;			/* maximum duration */
	int run;			/* whether to run this test */
	int result;			/* results of test */
	int expect;			/* expected result */
} tests[] = {
	{
	numb_case_0_1, tgrp_case_0_1, sgrp_case_0_1, name_case_0_1, xtra_case_0_1, desc_case_0_1, sref_case_0_1, test_case_0_1, &begin_tests, &end_tests, 5000, 0, 0, __RESULT_INCONCLUSIVE,}, {
	numb_case_0_2_1, tgrp_case_0_2_1, sgrp_case_0_2_1, name_case_0_2_1, xtra_case_0_2_1, desc_case_0_2_1, sref_case_0_2_1, test_case_0_2_1, &begin_tests, &end_tests, 0, 0, 0, __RESULT_SUCCESS,}, {
	numb_case_0_2_2, tgrp_case_0_2_2, sgrp_case_0_2_2, name_case_0_2_2, xtra_case_0_2_2, desc_case_0_2_2, sref_case_0_2_2, test_case_0_2_2, &begin_tests, &end_tests, 0, 0, 0, __RESULT_SUCCESS,}, {
	numb_case_0_2_3, tgrp_case_0_2_3, sgrp_case_0_2_3, name_case_0_2_3, xtra_case_0_2_3, desc_case_0_2_3, sref_case_0_2_3, test_case_0_2_3, &begin_tests, &end_tests, 0, 0, 0, __RESULT_SUCCESS,}, {
	numb_case_0_2_4, tgrp_case_0_2_4, sgrp_case_0_2_4, name_case_0_2_4, xtra_case_0_2_4, desc_case_0_2_4, sref_case_0_2_4, test_case_0_2_4, &begin_tests, &end_tests, 0, 0, 0, __RESULT_FAILURE,}, {
	numb_case_0_2_5, tgrp_case_0_2_5, sgrp_case_0_2_5, name_case_0_2_5, xtra_case_0_2_5, desc_case_0_2_5, sref_case_0_2_5, test_case_0_2_5, &begin_tests, &end_tests, 0, 0, 0, __RESULT_SUCCESS,}, {
	numb_case_0_2_6, tgrp_case_0_2_6, sgrp_case_0_2_6, name_case_0_2_6, xtra_case_0_2_6, desc_case_0_2_6, sref_case_0_2_6, test_case_0_2_6, &begin_tests, &end_tests, 0, 0, 0, __RESULT_SUCCESS,}, {
	numb_case_0_2_7, tgrp_case_0_2_7, sgrp_case_0_2_7, name_case_0_2_7, xtra_case_0_2_7, desc_case_0_2_7, sref_case_0_2_7, test_case_0_2_7, &begin_tests, &end_tests, 0, 0, 0, __RESULT_SUCCESS,}, {
	NULL,}
};

static int summary = 0;

void
print_header(void)
{
	if (verbose <= 0)
		return;
	dummy = lockf(fileno(stdout), F_LOCK, 0);
	fprintf(stdout, "\n%s - %s - %s - Conformance Test Suite\n", lstdname, lpkgname, shortname);
	fflush(stdout);
	dummy = lockf(fileno(stdout), F_ULOCK, 0);
}

int
do_tests(int num_tests)
{
	int i;
	int result = __RESULT_INCONCLUSIVE;
	int notapplicable = 0;
	int inconclusive = 0;
	int successes = 0;
	int failures = 0;
	int skipped = 0;
	int notselected = 0;
	int aborted = 0;
	int repeat = 0;
	int oldverbose = verbose;

	print_header();
	show = 0;
	if (verbose > 0) {
		dummy = lockf(fileno(stdout), F_LOCK, 0);
		fprintf(stdout, "\nUsing device %s\n\n", devname);
		fflush(stdout);
		dummy = lockf(fileno(stdout), F_ULOCK, 0);
	}
	if (num_tests == 1 || begin_tests(0) == __RESULT_SUCCESS) {
		if (num_tests != 1)
			end_tests(0);
		show = 1;
		for (i = 0; i < (sizeof(tests) / sizeof(struct test_case)) && tests[i].numb; i++) {
		      rerun:
			if (!tests[i].run) {
				tests[i].result = __RESULT_INCONCLUSIVE;
				notselected++;
				continue;
			}
			if (aborted) {
				tests[i].result = __RESULT_INCONCLUSIVE;
				inconclusive++;
				continue;
			}
			if (verbose > 0) {
				dummy = lockf(fileno(stdout), F_LOCK, 0);
				if (verbose > 1 && tests[i].tgrp)
					fprintf(stdout, "\nTest Group: %s", tests[i].tgrp);
				if (verbose > 1 && tests[i].sgrp)
					fprintf(stdout, "\nTest Subgroup: %s", tests[i].sgrp);
				if (tests[i].xtra)
					fprintf(stdout, "\nTest Case %s-%s/%s: %s (%s)\n", sstdname, shortname, tests[i].numb, tests[i].name, tests[i].xtra);
				else
					fprintf(stdout, "\nTest Case %s-%s/%s: %s\n", sstdname, shortname, tests[i].numb, tests[i].name);
				if (verbose > 1 && tests[i].sref)
					fprintf(stdout, "Test Reference: %s\n", tests[i].sref);
				if (verbose > 1 && tests[i].desc)
					fprintf(stdout, "%s\n", tests[i].desc);
				fprintf(stdout, "\n");
				fflush(stdout);
				dummy = lockf(fileno(stdout), F_ULOCK, 0);
			}
			if ((result = tests[i].result) == 0) {
				ulong duration = test_duration;

				if (duration > tests[i].duration) {
					if (tests[i].duration && duration > tests[i].duration)
						duration = tests[i].duration;
					if ((result = (*tests[i].start) (i)) != __RESULT_SUCCESS)
						goto inconclusive;
					result = test_run(tests[i].stream, duration);
					(*tests[i].stop) (i);
				} else
					result = __RESULT_SKIPPED;
				if (result == tests[i].expect) {
					switch (result) {
					case __RESULT_SUCCESS:
					case __RESULT_NOTAPPL:
					case __RESULT_SKIPPED:
						/* autotest can handle these */
						break;
					default:
					case __RESULT_INCONCLUSIVE:
					case __RESULT_FAILURE:
						/* these are expected failures */
						result = __RESULT_SUCCESS;
						break;
					}
				}
			} else {
				if (result == tests[i].expect) {
					switch (result) {
					case __RESULT_SUCCESS:
					case __RESULT_NOTAPPL:
					case __RESULT_SKIPPED:
						/* autotest can handle these */
						break;
					default:
					case __RESULT_INCONCLUSIVE:
					case __RESULT_FAILURE:
						/* these are expected failures */
						result = __RESULT_SUCCESS;
						break;
					}
				}
				switch (result) {
				case __RESULT_SUCCESS:
					print_passed(3);
					break;
				case __RESULT_FAILURE:
					print_failed(3);
					break;
				case __RESULT_NOTAPPL:
					print_notapplicable(3);
					break;
				case __RESULT_SKIPPED:
					print_skipped(3);
					break;
				default:
				case __RESULT_INCONCLUSIVE:
					print_inconclusive(3);
					break;
				}
			}
			switch (result) {
			case __RESULT_SUCCESS:
				successes++;
				if (verbose > 0) {
					dummy = lockf(fileno(stdout), F_LOCK, 0);
					fprintf(stdout, "\n");
					fprintf(stdout, "*********\n");
					fprintf(stdout, "********* Test Case SUCCESSFUL\n");
					fprintf(stdout, "*********\n\n");
					fflush(stdout);
					dummy = lockf(fileno(stdout), F_ULOCK, 0);
				}
				break;
			case __RESULT_FAILURE:
				if (!repeat_verbose || repeat)
					failures++;
				if (verbose > 0) {
					dummy = lockf(fileno(stdout), F_LOCK, 0);
					fprintf(stdout, "\n");
					fprintf(stdout, "XXXXXXXXX\n");
					fprintf(stdout, "XXXXXXXXX Test Case FAILED\n");
					fprintf(stdout, "XXXXXXXXX\n\n");
					fflush(stdout);
					dummy = lockf(fileno(stdout), F_ULOCK, 0);
				}
				break;
			case __RESULT_NOTAPPL:
				notapplicable++;
				if (verbose > 0) {
					dummy = lockf(fileno(stdout), F_LOCK, 0);
					fprintf(stdout, "\n");
					fprintf(stdout, "XXXXXXXXX\n");
					fprintf(stdout, "XXXXXXXXX Test Case NOT APPLICABLE\n");
					fprintf(stdout, "XXXXXXXXX\n\n");
					fflush(stdout);
					dummy = lockf(fileno(stdout), F_ULOCK, 0);
				}
				break;
			case __RESULT_SKIPPED:
				skipped++;
				if (verbose > 0) {
					dummy = lockf(fileno(stdout), F_LOCK, 0);
					fprintf(stdout, "\n");
					fprintf(stdout, "XXXXXXXXX\n");
					fprintf(stdout, "XXXXXXXXX Test Case SKIPPED\n");
					fprintf(stdout, "XXXXXXXXX\n\n");
					fflush(stdout);
					dummy = lockf(fileno(stdout), F_ULOCK, 0);
				}
				break;
			default:
			case __RESULT_INCONCLUSIVE:
			      inconclusive:
				if (!repeat_verbose || repeat)
					inconclusive++;
				if (verbose > 0) {
					dummy = lockf(fileno(stdout), F_LOCK, 0);
					fprintf(stdout, "\n");
					fprintf(stdout, "?????????\n");
					fprintf(stdout, "????????? Test Case INCONCLUSIVE\n");
					fprintf(stdout, "?????????\n\n");
					fflush(stdout);
					dummy = lockf(fileno(stdout), F_ULOCK, 0);
				}
				break;
			}
			if (repeat_on_failure && (result == __RESULT_FAILURE || result == __RESULT_INCONCLUSIVE))
				goto rerun;
			if (repeat_on_success && (result == __RESULT_SUCCESS))
				goto rerun;
			if (repeat) {
				repeat = 0;
				verbose = oldverbose;
			} else if (repeat_verbose && (result == __RESULT_FAILURE || result == __RESULT_INCONCLUSIVE)) {
				repeat = 1;
				verbose = 5;
				goto rerun;
			}
			tests[i].result = result;
			if (exit_on_failure && (result == __RESULT_FAILURE || result == __RESULT_INCONCLUSIVE))
				aborted = 1;
		}
		if (summary && verbose) {
			dummy = lockf(fileno(stdout), F_LOCK, 0);
			fprintf(stdout, "\n");
			fflush(stdout);
			dummy = lockf(fileno(stdout), F_ULOCK, 0);
			for (i = 0; i < (sizeof(tests) / sizeof(struct test_case)) && tests[i].numb; i++) {
				if (tests[i].run) {
					dummy = lockf(fileno(stdout), F_LOCK, 0);
					fprintf(stdout, "Test Case %s-%s/%-10s ", sstdname, shortname, tests[i].numb);
					fflush(stdout);
					dummy = lockf(fileno(stdout), F_ULOCK, 0);
					switch (tests[i].result) {
					case __RESULT_SUCCESS:
						dummy = lockf(fileno(stdout), F_LOCK, 0);
						fprintf(stdout, "SUCCESS\n");
						fflush(stdout);
						dummy = lockf(fileno(stdout), F_ULOCK, 0);
						break;
					case __RESULT_FAILURE:
						dummy = lockf(fileno(stdout), F_LOCK, 0);
						fprintf(stdout, "FAILURE\n");
						fflush(stdout);
						dummy = lockf(fileno(stdout), F_ULOCK, 0);
						break;
					case __RESULT_NOTAPPL:
						dummy = lockf(fileno(stdout), F_LOCK, 0);
						fprintf(stdout, "NOT APPLICABLE\n");
						fflush(stdout);
						dummy = lockf(fileno(stdout), F_ULOCK, 0);
						break;
					case __RESULT_SKIPPED:
						dummy = lockf(fileno(stdout), F_LOCK, 0);
						fprintf(stdout, "SKIPPED\n");
						fflush(stdout);
						dummy = lockf(fileno(stdout), F_ULOCK, 0);
						break;
					default:
					case __RESULT_INCONCLUSIVE:
						dummy = lockf(fileno(stdout), F_LOCK, 0);
						fprintf(stdout, "INCONCLUSIVE\n");
						fflush(stdout);
						dummy = lockf(fileno(stdout), F_ULOCK, 0);
						break;
					}
				}
			}
		}
		if (verbose > 0 && num_tests > 1) {
			dummy = lockf(fileno(stdout), F_LOCK, 0);
			fprintf(stdout, "\n");
			fprintf(stdout, "========= %3d successes     \n", successes);
			fprintf(stdout, "========= %3d failures      \n", failures);
			fprintf(stdout, "========= %3d inconclusive  \n", inconclusive);
			fprintf(stdout, "========= %3d not applicable\n", notapplicable);
			fprintf(stdout, "========= %3d skipped       \n", skipped);
			fprintf(stdout, "========= %3d not selected  \n", notselected);
			fprintf(stdout, "============================\n");
			fprintf(stdout, "========= %3d total         \n", successes + failures + inconclusive + notapplicable + skipped + notselected);
			if (!(aborted + failures))
				fprintf(stdout, "\nDone.\n\n");
			fflush(stdout);
			dummy = lockf(fileno(stdout), F_ULOCK, 0);
		}
		if (aborted) {
			dummy = lockf(fileno(stderr), F_LOCK, 0);
			if (verbose > 0)
				fprintf(stderr, "\n");
			fprintf(stderr, "Test Suite aborted due to failure.\n");
			if (verbose > 0)
				fprintf(stderr, "\n");
			fflush(stderr);
			dummy = lockf(fileno(stderr), F_ULOCK, 0);
		} else if (failures) {
			dummy = lockf(fileno(stderr), F_LOCK, 0);
			if (verbose > 0)
				fprintf(stderr, "\n");
			fprintf(stderr, "Test Suite failed.\n");
			if (verbose > 0)
				fprintf(stderr, "\n");
			fflush(stderr);
			dummy = lockf(fileno(stderr), F_ULOCK, 0);
		}
		if (num_tests == 1) {
			if (successes)
				return (0);
			if (failures)
				return (1);
			if (inconclusive)
				return (1);
			if (notapplicable)
				return (0);
			if (skipped)
				return (77);
		}
		return (aborted);
	} else {
		end_tests(0);
		show = 1;
		dummy = lockf(fileno(stderr), F_LOCK, 0);
		fprintf(stderr, "Test Suite setup failed!\n");
		fflush(stderr);
		dummy = lockf(fileno(stderr), F_ULOCK, 0);
		return (2);
	}
}

void
copying(int argc, char *argv[])
{
	if (verbose <= 0)
		return;
	print_header();
	fprintf(stdout, "\
\n\
Copyright (c) 2008-2015  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>\n\
Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>\n\
\n\
All Rights Reserved.\n\
\n\
Unauthorized distribution or duplication is prohibited.\n\
\n\
This software and related documentation is protected by copyright and distribut-\n\
ed under licenses restricting its use,  copying, distribution and decompilation.\n\
No part of this software or related documentation may  be reproduced in any form\n\
by any means without the prior  written  authorization of the  copyright holder,\n\
and licensors, if any.\n\
\n\
The recipient of this document,  by its retention and use, warrants that the re-\n\
cipient  will protect this  information and  keep it confidential,  and will not\n\
disclose the information contained  in this document without the written permis-\n\
sion of its owner.\n\
\n\
The author reserves the right to revise  this software and documentation for any\n\
reason,  including but not limited to, conformity with standards  promulgated by\n\
various agencies, utilization of advances in the state of the technical arts, or\n\
the reflection of changes  in the design of any techniques, or procedures embod-\n\
ied, described, or  referred to herein.   The author  is under no  obligation to\n\
provide any feature listed herein.\n\
\n\
As an exception to the above,  this software may be  distributed  under the  GNU\n\
Affero  General  Public  License  (AGPL)  Version  3, so long as the software is\n\
distributed with, and only used for the testing of, OpenSS7 modules, drivers and\n\
libraries.\n\
\n\
U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on behalf\n\
of the  U.S. Government  (\"Government\"),  the following provisions apply to you.\n\
If the Software is  supplied by the Department of Defense (\"DoD\"), it is classi-\n\
fied as  \"Commercial Computer Software\"  under paragraph 252.227-7014 of the DoD\n\
Supplement  to the  Federal Acquisition Regulations  (\"DFARS\") (or any successor\n\
regulations) and the  Government  is acquiring  only the license rights  granted\n\
herein (the license  rights customarily  provided to non-Government  users).  If\n\
the Software is supplied to any unit or agency of the Government other than DoD,\n\
it is classified as  \"Restricted Computer Software\" and the  Government's rights\n\
in the  Software are defined in  paragraph 52.227-19 of the Federal  Acquisition\n\
Regulations  (\"FAR\") (or any successor regulations) or, in the cases of NASA, in\n\
paragraph  18.52.227-86 of the  NASA Supplement  to the  FAR (or  any  successor\n\
regulations).\n\
\n\
");
	fflush(stdout);
}

void
version(int argc, char *argv[])
{
	if (verbose <= 0)
		return;
	fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s (%4$s)\n\
Written by Brian Bidulock\n\
\n\
Copyright (c) 2008, 2009, 2010, 2015  Monavacon Limited.\n\
Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008  OpenSS7 Corporation.\n\
Copyright (c) 1997, 1998, 1999, 2000, 2001  Brian F. G. Bidulock.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n\
Distributed by OpenSS7 Corporation under GNU Affero General Public License Version 3,\n\
incorporated herein by reference.  See `%1$s --copying' for copying permissions.\n\
", NAME, PACKAGE, VERSION, PACKAGE_ENVR " " PACKAGE_DATE);
}

void
usage(int argc, char *argv[])
{
	if (verbose <= 0)
		return;
	fprintf(stderr, "\
Usage:\n\
    %1$s [options]\n\
    %1$s {-h, --help}\n\
    %1$s {-V, --version}\n\
    %1$s {-C, --copying}\n\
", argv[0]);
}

void
help(int argc, char *argv[])
{
	if (verbose <= 0)
		return;
	fprintf(stdout, "\
\n\
Usage:\n\
    %1$s [options]\n\
    %1$s {-h, --help}\n\
    %1$s {-V, --version}\n\
    %1$s {-C, --copying}\n\
Arguments:\n\
    (none)\n\
Options:\n\
    -u, --iut\n\
        IUT connects instead of PT.\n\
    -D, --decl-std [STANDARD]\n\
        specify the SS7 standard version to test.\n\
    -c, --client\n\
        execute client side (PTU) of test case only.\n\
    -S, --server\n\
        execute server side (IUT) of test case only.\n\
    -a, --again\n\
        repeat failed tests verbose.\n\
    -w, --wait\n\
        have server wait indefinitely.\n\
    -r, --repeat\n\
        repeat test cases on success or failure.\n\
    -R, --repeat-fail\n\
        repeat test cases on failure.\n\
    -p, --client-port [PORT]\n\
        port number from which to connect [default: %3$d+index*3]\n\
    -P, --server-port [PORT]\n\
        port number to which to connect or upon which to listen\n\
        [default: %3$d+index*3+2]\n\
    -i, --client-host [HOSTNAME[,HOSTNAME]*]\n\
        client host names(s) or IP numbers\n\
        [default: 127.0.0.1,127.0.0.2,127.0.0.3]\n\
    -I, --server-host [HOSTNAME[,HOSTNAME]*]\n\
        server host names(s) or IP numbers\n\
        [default: 127.0.0.1,127.0.0.2,127.0.0.3]\n\
    -d, --device DEVICE\n\
        device name to open [default: %2$s].\n\
    -e, --exit\n\
        exit on the first failed or inconclusive test case.\n\
    -l, --list [RANGE]\n\
        list test case names within a range [default: all] and exit.\n\
    -f, --fast [SCALE]\n\
        increase speed of tests by scaling timers [default: 50]\n\
    -s, --summary\n\
        print a test case summary at end of testing [default: off]\n\
    -o, --onetest [TESTCASE]\n\
        run a single test case.\n\
    -t, --tests [RANGE]\n\
        run a range of test cases.\n\
    -m, --messages\n\
        display messages. [default: off]\n\
    -q, --quiet\n\
        suppress normal output (equivalent to --verbose=0)\n\
    -v, --verbose [LEVEL]\n\
        increase verbosity or set to LEVEL [default: 1]\n\
        this option may be repeated.\n\
    -h, --help, -?, --?\n\
        print this usage message and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
Symbols:\n\
    [STANDARD]\n\
        SS7_PVAR_ITUT_83  SS7_PVAR_ETSI_88  SS7_PVAR_ANSI_92  SS7_PVAR_SING_88\n\
        SS7_PVAR_ITUT_93  SS7_PVAR_ETSI_93  SS7_PVAR_JTTC_94  SS7_PVAR_SPAN_88\n\
        SS7_PVAR_ITUT_96  SS7_PVAR_ETSI_96  SS7_PVAR_ANSI_96\n\
        SS7_PVAR_ITUT_00  SS7_PVAR_ETSI_00  SS7_PVAR_ANSI_00  SS7_PVAR_CHIN_00\n\
\n\
", argv[0], devname, TEST_PORT_NUMBER);
}

#define HOST_BUF_LEN 128

int
main(int argc, char *argv[])
{
	size_t l, n;
	int range = 0;
	struct test_case *t;
	int tests_to_run = 0;

	for (t = tests; t->numb; t++) {
		if (!t->result) {
			t->run = 1;
			tests_to_run++;
		}
	}
	for (;;) {
		int c, val;

#if defined _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"iut",		no_argument,		NULL, 'u'},
			{"draft",	required_argument,	NULL, 'D'},
			{"decl-std",	required_argument,	NULL, 'D'},
			{"client",	no_argument,		NULL, 'c'},
			{"server",	no_argument,		NULL, 'S'},
			{"again",	no_argument,		NULL, 'a'},
			{"wait",	no_argument,		NULL, 'w'},
			{"client-port",	required_argument,	NULL, 'p'},
			{"server-port",	required_argument,	NULL, 'P'},
			{"client-host",	required_argument,	NULL, 'i'},
			{"server-host",	required_argument,	NULL, 'I'},
			{"repeat",	no_argument,		NULL, 'r'},
			{"repeat-fail",	no_argument,		NULL, 'R'},
			{"device",	required_argument,	NULL, 'd'},
			{"exit",	no_argument,		NULL, 'e'},
			{"list",	optional_argument,	NULL, 'l'},
			{"fast",	optional_argument,	NULL, 'f'},
			{"summary",	no_argument,		NULL, 's'},
			{"onetest",	required_argument,	NULL, 'o'},
			{"tests",	required_argument,	NULL, 't'},
			{"messages",	no_argument,		NULL, 'm'},
			{"quiet",	no_argument,		NULL, 'q'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'h'},
			{NULL,		0,			NULL,  0 }
		};
		/* *INDENT-ON* */

		c = getopt_long(argc, argv, "uD:cSawp:P:i:I:rRd:el::f::so:t:mqvhVC?", long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "uD:cSawp:P:i:I:rRd:el::f::so:t:mqvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1)
			break;
		switch (c) {
		case 'u':	/* --iut */
			iut_connects = 1;
			break;
		case 'D':	/* --decl-std */
			ss7_pvar = strtoul(optarg, NULL, 0);
			switch (ss7_pvar) {
			case SS7_PVAR_ITUT_88:
			case SS7_PVAR_ITUT_93:
			case SS7_PVAR_ITUT_96:
			case SS7_PVAR_ITUT_00:
			case SS7_PVAR_ETSI_88:
			case SS7_PVAR_ETSI_93:
			case SS7_PVAR_ETSI_96:
			case SS7_PVAR_ETSI_00:
			case SS7_PVAR_ANSI_92:
			case SS7_PVAR_ANSI_96:
			case SS7_PVAR_ANSI_00:
			case SS7_PVAR_JTTC_94:
			case SS7_PVAR_CHIN_00:
			case SS7_PVAR_SING | SS7_PVAR_88:
			case SS7_PVAR_SPAN | SS7_PVAR_88:
				break;
			default:
				if (!strcmp(optarg, "SS7_PVAR_ITUT_88"))
					ss7_pvar = SS7_PVAR_ITUT_88;
				else if (!strcmp(optarg, "SS7_PVAR_ITUT_93"))
					ss7_pvar = SS7_PVAR_ITUT_93;
				else if (!strcmp(optarg, "SS7_PVAR_ITUT_96"))
					ss7_pvar = SS7_PVAR_ITUT_96;
				else if (!strcmp(optarg, "SS7_PVAR_ITUT_00"))
					ss7_pvar = SS7_PVAR_ITUT_00;
				else if (!strcmp(optarg, "SS7_PVAR_ETSI_88"))
					ss7_pvar = SS7_PVAR_ETSI_88;
				else if (!strcmp(optarg, "SS7_PVAR_ETSI_93"))
					ss7_pvar = SS7_PVAR_ETSI_93;
				else if (!strcmp(optarg, "SS7_PVAR_ETSI_96"))
					ss7_pvar = SS7_PVAR_ETSI_96;
				else if (!strcmp(optarg, "SS7_PVAR_ETSI_00"))
					ss7_pvar = SS7_PVAR_ETSI_00;
				else if (!strcmp(optarg, "SS7_PVAR_ANSI_92"))
					ss7_pvar = SS7_PVAR_ANSI_92;
				else if (!strcmp(optarg, "SS7_PVAR_ANSI_96"))
					ss7_pvar = SS7_PVAR_ANSI_96;
				else if (!strcmp(optarg, "SS7_PVAR_ANSI_00"))
					ss7_pvar = SS7_PVAR_ANSI_00;
				else if (!strcmp(optarg, "SS7_PVAR_JTTC_94"))
					ss7_pvar = SS7_PVAR_JTTC_94;
				else if (!strcmp(optarg, "SS7_PVAR_CHIN_00"))
					ss7_pvar = SS7_PVAR_CHIN_00;
				else if (!strcmp(optarg, "SS7_PVAR_SING_88"))
					ss7_pvar = SS7_PVAR_SING | SS7_PVAR_88;
				else if (!strcmp(optarg, "SS7_PVAR_SPAN_88"))
					ss7_pvar = SS7_PVAR_SPAN | SS7_PVAR_88;
				else
					goto bad_option;
			}
			break;
		case 'c':	/* --client */
			client_exec = 1;
			break;
		case 'S':	/* --server */
			server_exec = 1;
			break;
		case 'a':	/* --again */
			repeat_verbose = 1;
			break;
		case 'w':	/* --wait */
			test_duration = INFINITE_WAIT;
			break;
		case 'r':	/* --repeat */
			repeat_on_success = 1;
			repeat_on_failure = 1;
			break;
		case 'R':	/* --repeat-fail */
			repeat_on_failure = 1;
			break;
		case 'd':
			if (optarg) {
				snprintf(devname, sizeof(devname), "%s", optarg);
				break;
			}
			goto bad_option;
		case 'e':
			exit_on_failure = 1;
			break;
		case 'l':
			if (optarg) {
				l = strnlen(optarg, 16);
				fprintf(stdout, "\n");
				for (n = 0, t = tests; t->numb; t++)
					if (!strncmp(t->numb, optarg, l)) {
						if (verbose > 2 && t->tgrp)
							fprintf(stdout, "Test Group: %s\n", t->tgrp);
						if (verbose > 2 && t->sgrp)
							fprintf(stdout, "Test Subgroup: %s\n", t->sgrp);
						if (t->xtra)
							fprintf(stdout, "Test Case %s-%s/%s: %s (%s)\n", sstdname, shortname, t->numb, t->name, t->xtra);
						else
							fprintf(stdout, "Test Case %s-%s/%s: %s\n", sstdname, shortname, t->numb, t->name);
						if (verbose > 2 && t->sref)
							fprintf(stdout, "Test Reference: %s\n", t->sref);
						if (verbose > 1 && t->desc)
							fprintf(stdout, "%s\n\n", t->desc);
						fflush(stdout);
						n++;
					}
				if (!n) {
					fprintf(stderr, "WARNING: specification `%s' matched no test\n", optarg);
					fflush(stderr);
					goto bad_option;
				}
				if (verbose <= 1)
					fprintf(stdout, "\n");
				fflush(stdout);
				exit(0);
			} else {
				fprintf(stdout, "\n");
				for (t = tests; t->numb; t++) {
					if (verbose > 2 && t->tgrp)
						fprintf(stdout, "Test Group: %s\n", t->tgrp);
					if (verbose > 2 && t->sgrp)
						fprintf(stdout, "Test Subgroup: %s\n", t->sgrp);
					if (t->xtra)
						fprintf(stdout, "Test Case %s-%s/%s: %s (%s)\n", sstdname, shortname, t->numb, t->name, t->xtra);
					else
						fprintf(stdout, "Test Case %s-%s/%s: %s\n", sstdname, shortname, t->numb, t->name);
					if (verbose > 2 && t->sref)
						fprintf(stdout, "Test Reference: %s\n", t->sref);
					if (verbose > 1 && t->desc)
						fprintf(stdout, "%s\n\n", t->desc);
					fflush(stdout);
				}
				if (verbose <= 1)
					fprintf(stdout, "\n");
				fflush(stdout);
				exit(0);
			}
			break;
		case 'f':
			if (optarg)
				timer_scale = atoi(optarg);
			else
				timer_scale = 50;
			fprintf(stderr, "WARNING: timers are scaled by a factor of %ld\n", (long) timer_scale);
			break;
		case 's':
			summary = 1;
			break;
		case 'o':
			if (optarg) {
				if (!range) {
					for (t = tests; t->numb; t++)
						t->run = 0;
					tests_to_run = 0;
				}
				range = 1;
				for (n = 0, t = tests; t->numb; t++)
					if (!strncmp(t->numb, optarg, 16)) {
						// if (!t->result) {
						t->run = 1;
						n++;
						tests_to_run++;
						// }
					}
				if (!n) {
					fprintf(stderr, "WARNING: specification `%s' matched no test\n", optarg);
					fflush(stderr);
					goto bad_option;
				}
				break;
			}
			goto bad_option;
		case 't':
			l = strnlen(optarg, 16);
			if (!range) {
				for (t = tests; t->numb; t++)
					t->run = 0;
				tests_to_run = 0;
			}
			range = 1;
			for (n = 0, t = tests; t->numb; t++)
				if (!strncmp(t->numb, optarg, l)) {
					// if (!t->result) {
					t->run = 1;
					n++;
					tests_to_run++;
					// }
				}
			if (!n) {
				fprintf(stderr, "WARNING: specification `%s' matched no test\n", optarg);
				fflush(stderr);
				goto bad_option;
			}
			break;
		case 'm':
			show_msg = 1;
			break;
		case 'q':	/* -q, --quiet */
			verbose = 0;
			break;
		case 'v':	/* -v, --verbose */
			if (optarg == NULL) {
				verbose++;
				show_msg++;
				break;
			}
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			verbose = val;
			show_msg = val;
			break;
		case 'H':	/* -H */
		case 'h':	/* -h, --help */
			help(argc, argv);
			exit(0);
		case 'V':	/* -V, --version */
			version(argc, argv);
			exit(0);
		case 'C':
			copying(argc, argv);
			exit(0);
		case '?':
		default:
		      bad_option:
			optind--;
		      bad_nonopt:
			if (optind < argc && verbose) {
				fprintf(stderr, "%s: illegal syntax -- ", argv[0]);
				while (optind < argc)
					fprintf(stderr, "%s ", argv[optind++]);
				fprintf(stderr, "\n");
				fflush(stderr);
			}
			goto bad_usage;
		      bad_usage:
			usage(argc, argv);
			exit(2);
		}
	}
	/* 
	 * dont' ignore non-option arguments
	 */
	if (optind < argc)
		goto bad_nonopt;
	switch (tests_to_run) {
	case 0:
		if (verbose > 0) {
			fprintf(stderr, "%s: error: no tests to run\n", argv[0]);
			fflush(stderr);
		}
		exit(2);
	case 1:
		break;
	default:
		copying(argc, argv);
	}
	if (client_exec == 0 && server_exec == 0) {
		client_exec = 1;
		server_exec = 1;
	}
#if 0
	if (client_ppa_specified) {
	}
	if (server_ppa_specified) {
	}
#endif
	exit(do_tests(tests_to_run));
}
