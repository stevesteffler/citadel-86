
/*
 *				SysDep.H
 *
 * #include file for all Citadel C files; contains system dependent code.
 */

#ifndef SYSDEP_HEADER

#define SYSDEP_HEADER
/*
 * This is here because Borland International requires all licensees to place
 * copyrights on any distributable s/w.  If you are porting C-86 to another
 * machine, you should certainly replace the value here with something else.
 * A "" is probably acceptable.
 */

#define COPYRIGHT "Copyright (c) 1988 - 1998 by Hue, Jr."

#define TURBO_C

#ifdef TURBO_C

#ifndef NO_EXTRA_HEADERS

#include "stdlib.h"
#include "dos.h"
#include "dir.h"
#include "string.h"
#include "mem.h"
#include "io.h"
#include "conio.h"
#include "keystrk.h"

#endif

#include "ctype.h"

#include "citcolor.h"

#define ANSI_PROTOTYPING
#define TURBO_C_VSPRINTF_BUG

#endif

#define SYSTEM_CLOCK
/* #define DeARC_SUPPORT        now defaulting to true */
#define OUTSIDE_EDITOR_SUPPORT

#define VARIANT_NAME	"Citadel-86"

/*
 * These constants, etc. are only for use by SYSDEP.C and other source
 * files that contain code that depends on the computer system in use.
 * Access is via #define in the appropriate source files.
 */

/*
 * Required! The following MUST be defined:
 *   MSG_SECT_SIZE -- size, in bytes, of a (psuedo) "sector" on disk
 *                    for the message file
 *   SIZE_SYS_FILE -- longest length, in bytes, of a "system" (i.e., *.SYS,
 *                    *.BLB, *.MNU, *.HLP, and net files) file plus 1 (for
 *                    the NULL byte.  Variables using this constant are used for
 *                    defining a name of a file and then opening it.
 *   MSG_NUMBER  -- just what is* a message number
 *   SECTOR_ID   -- just what to use to identify a sector
 *   AN_UNSIGNED -- an unsigned quantity, very preferably 8 bits
 *   ROOM_MSG    -- this needs* to be 16 bits, for a kludge in listRooms()
 *   DATA_BLOCK[MSG_SECT_SIZE] -- a block of msg data from disk
 *   CRC_TYPE    -- sufficient to hold a XMODEM CRC value
 *   SYS_FILE    -- a variable used in makeSysName() that holds the "name" of
 *                  a system file (see SIZE_SYS_FILE).  This is passed to
 *                  safeopen().  Normally, this would be just a string using
 *                  the SIZE_SYS_FILE constant, but I can see situations in
 *                  which a struct would be more appropriate.  If you have*
 *                  to go with a struct, I suggest making safeopen() a macro
 *                  that takes the address of the name (see description of
 *                  safeopen for its function).  I.e...
 *  #define safeopen(x, y)    ourOpen(&x, y)
 *   NET_AREA    -- definition of an area accessible to net functions
 *   SYS_AREA    -- definition of an area that may be occupied by a Citadel
 *                  system file.
 *   MODEM_DATA  -- all necessary modem data for this implementation
 *   MenuId      -- this should be a typedef if you're doing the special menu
 *                  handling for the system console.
 *
 */

#define MSG_SECT_SIZE   128
#define SIZE_SYS_FILE   31              /* Should be enough             */
#define MAX_FILENAME	15		/* include eos byte		*/

#define LOCKFILE        "ctdllock.sys"

#define ALL_FILES	"*.*"
#define CACHED_FILES	"*.msg"

#define COMMENT_HEADER		"filedir.inf"

typedef unsigned long	MSG_NUMBER;       /* Msg number for PClone 32 bits*/
typedef unsigned	SECTOR_ID;        /* Sector ID -- 16 bits         */
typedef unsigned char	AN_UNSIGNED;
typedef unsigned char	DATA_BLOCK[MSG_SECT_SIZE];
typedef unsigned	CRC_TYPE;
typedef char		SYS_FILE[SIZE_SYS_FILE];
typedef char		DOMAIN_FILE[40];
typedef unsigned long	MULTI_NET_DATA;
typedef unsigned long	UNS_32;
typedef unsigned short	UNS_16;
typedef int		MenuId;
typedef struct {
    char *CmdLine;
} SystemProtocol;
#define NO_MENU		-1

#define S_MSG_MASK	0x7fffffffl	/* hi bit, right?	*/

typedef struct {
    char        naDisk;         /* Disk on MS-DOS                       */
    char        naDirname[100]; /* Directory anywhere on system         */
} NET_AREA;

typedef struct {
    char        saDisk;         /* Disk on MS-DOS                       */
    int         saDirname;      /* Points into cfg.codeBuf              */
} SYS_AREA;

typedef struct {
    ScreenMap ScreenColors;     /* map of colors                        */
    char IBM,                   /* Is this a PClone                     */
	 LockPort,
         OldVideo;
    char Editor[15],
         EditArea[40],
         HiSpeedInit[30];
#define NO_CLOCK        0
#define BUSY_CLOCK      1
#define ALWAYS_CLOCK    2
    int  Clock;
    int  modem_status,          /* MSR location                         */
         modem_data,            /* Data location                        */
         line_status,           /* LSR location                         */
         mdm_ctrl,              /* MCR location                         */
         ln_ctrl,               /* LCR location                         */
         ier,                   /* Interrupt Enable location            */
         com_vector,            /* Com vector                           */
         PIC_mask,              /* For the Interrupt Controller         */
	 pInitString,
	 InterCharDelay,	/* modem offline commands */
	 DialPrefixes[9];
    UNS_16  BannerCount;
    char sEnable[15],
	 sDisable[15];
} DependentData;

#define GetPrefix(n)	cfg.codeBuf + cfg.DepData.DialPrefixes[(n)]

#define MNP			0x01

#define BOOLEAN_FLAG(x)         unsigned x : 1

/* Optional gunk for this port only. */
void MoveToSysDirectory(SYS_AREA *area);

#define MoveToBioDirectory()	MoveToSysDirectory(&cfg.bioArea);
#define MoveToInfoDirectory()	MoveToSysDirectory(&cfg.infoArea);

#define mvToHomeDisk(x)         DoBdos(14, (x)->saDisk)
#define simpleGetch()           DoBdos(7, 0)

#define makeSysName(x, y, z)    sprintf(x, "%c:%s%s", (z)->saDisk + 'a',\
                                  cfg.codeBuf + (z)->saDirname, y)

#define makeVASysName(x, y)     sprintf(x, "virtual\\%s", y)

#define zero_struct(x)  memset(&x, 0, sizeof x)
#define zero_array(x)   memset(x, 0, sizeof x)

#define copy_struct(src, dest)  memcpy(&dest, &src, sizeof src)
#define copy_array(src, dest)   memcpy(dest, src, sizeof src)
#define copy_ptr(src, dest, s)  memcpy(dest, src, (sizeof src[0]) * s)

#define CreateVAName(fn, slot, dir, num) \
                sprintf(fn, "virtual\\%d\\%s\\%ld", slot, dir, num)

#define ToTempArea()	MakeTempDir()
extern char TDirBuffer[];
#define KillTempArea()	homeSpace(), rmdir(TDirBuffer)
#define makeBanner(x, y)	sprintf(x, "banners\\%s.%d", y, CurAbsolute() % cfg.DepData.BannerCount)
#define NormalName(x, y)	sprintf(y, "%c:%s", (x)->snArea.naDisk, (x)->snArea.naDirname)
#define ReceptAreaPath(x, y)	sprintf(y, "%c:%s", 'a'+(x)->naDisk, (x)->naDirname)
#define RedirectName(b, d, f)	sprintf(b, "%s\\%s", d, f);

#define MakeDomainFileName(buffer, Dir, filename)	\
    sprintf(buffer, "%c:%s%d\\%s", cfg.domainArea.saDisk + 'a',\
                   cfg.codeBuf + cfg.domainArea.saDirname, Dir, filename)

#define MakeDomainDirectory(x)	DoDomainDirectory(x, FALSE)
#define KillDomainDirectory(x)	DoDomainDirectory(x, TRUE)

#define MilliSecPause(x)	delay(x)

#define KBReady()       kbhit()

#define totalBytes(x, fd)       *(x) = filelength(fileno(fd))
#define DoBdos(x, y)    bdos(x, y, 0)

#define ChatEat(c)	c == PG_DN
#define ChatSend(c)	c == PG_UP

#define NeedSysopInpPrompt()	(onConsole && !cfg.DepData.OldVideo)

#define VirtualCopyFileToFile(fn, vfn)		\
CitSystem(FALSE, "copy %s %s > nul", fn, vfn)

/*
 * These are for handling the net caching stuff.
 */
#define ChangeToCacheDir(x)	chdir(x)

#define NetCacheName(buf, slot, name)	\
sprintf(buf, "%c:%snetcache\\%d\\%s", cfg.netArea.saDisk + 'a', \
cfg.codeBuf + cfg.netArea.saDirname, slot, name)

/*
 * this is used for both mkdir() and chdir() calls.
 */
#define MakeNetCacheName(buf, slot)	\
sprintf(buf, "%c:%snetcache\\%d", cfg.netArea.saDisk + 'a', \
cfg.codeBuf + cfg.netArea.saDirname, slot)

/*
 * this, too
 */
#define MakeNetCache(buf)	\
sprintf(buf, "%c:%snetcache", cfg.netArea.saDisk + 'a', \
cfg.codeBuf + cfg.netArea.saDirname)

#define MakeDeCompressedFilename(fn, Fn, dir)	\
sprintf(fn, "%s\\%s", dir, Fn)

typedef struct {
	/* char *FileName; */
	char *MenuEntry;
	char *Suffix;
	char *DeWork;
	char *IntWork;
	char *CompWork;
	char *TOC;
} DeCompElement;

extern DeCompElement DeComp[];

#define GetCompEnglish(CompType)   DeComp[(CompType) - 1].MenuEntry
#define DeCompAvailable(CompType) (DeComp[(CompType) - 1].DeWork != NULL)
#define AnyCompression()	\
(DeComp[LHA_COMP-1].CompWork != NULL || DeComp[ARC_COMP-1].CompWork != NULL || \
DeComp[ZOO_COMP-1].CompWork != NULL || DeComp[ZIP_COMP-1].CompWork != NULL)

#define netSetNewArea(fd) realSetSpace((fd)->naDisk, (fd)->naDirname)

#undef inp

/*
 *			TI PC specific macros
 */
#define TiInternal	modem_status
#define TiComPort	modem_data

/*
 * These constants, etc. are only for use by SYSDEP.C and other source
 * files that contain code that depends on the computer system in use.
 * Access is via #define in the appropriate source files.
 */

#ifdef CONFIGURE

#define HELP		0
#define LOG		1
#define ROOM		2
#define MSG		3
#define MSG2		4
#define NET_STUFF	5
#define CALL		6
#define HOLD		7
#define FLOORA		8
#define DOMAIN_STUFF	9
#define INFO_STUFF	10

#define I_HANGUP        0
#define I_INIT          1
#define I_CARRDET       2
#define I_SET300        3
#define I_SET1200       4
#define I_SET2400       5
#define I_SETHI         6
#define I_CHECKB        7
#define I_ENABLE        8
#define I_DISABLE       9
#define I_SET4800       10
#define I_SET9600       11
#define I_SET14400	12
#define I_SET19200	13

#endif

#define BAD_DIR         -1      /* chdir() return value */

#define strCmp          strcmp
#define strLen          strlen
#define sPrintf         sprintf
#define toUpper         toupper
#define toLower         _tolower
#define strCpy          strcpy
#define strCat          strcat
#define isAlpha         isalpha
#define strnCmp         strncmp
#define isSpace         isspace

/* end of file */

#endif
