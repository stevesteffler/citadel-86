/*
 *				sysdep2.c
 *
 * This is the repository of most of the system dependent MS-DOS code in
 * Citadel.  We hope, pray, and proselytize, at least.  IBM & Z-100 specific
 * code located elsewhere.
 */

/*
 *				history
 *
 * 97Jan11 HAW  Added ReadMessageSpec.
 * 86Dec14 HAW  Reorganized into areas.
 * 86Nov25 HAW  Created.
 */

#define SYSTEM_DEPENDENT
#define TIMER_FUNCTIONS_NEEDED

#include "ctdl.h"
#include "sys\stat.h"
#include "ctype.h"
#include "time.h"
#include "stdarg.h"

/*
 *				Contents
 *
 *		SYSTEM FORMATTING:
 *	dPrintf()		printf() that writes to disk
 *	mPrintf()		writes a line to modem & console
 *	mTrPrintf()		special mPrintf for WC transfers
 * #    splitF()		debug formatter
 *		TIMERS:
 *	chkTimeSince()		check how long since timer initialized
 * #    milliTimeSince()		How long in milliseconds have passed
 *	pause()		pauses for N/100 seconds
 * #    setTimer()		start a specific timer.
 *	startTimer()		Initialize a general timer
 *	timeSince()		how long since general timer init
 *		CONSOLE STUFF, continued
 *	ScreenUser()		update fn for givePrompt()
 *	ScrNewUser()		status line updates
 *	SpecialMessage()		special status line messages
 *		MISCELLANEOUS:
 * #    diskSpaceLeft()		amount of space left on specified disk
 *	getRawDate()		gets date from system
 *	giveSpaceLeft()		give amount of space left on disk sys
 * #    initBadList()		read in list of bad filenames
 * #    interpret()		interprets a configuration routine
 * #    nodie()		for ^C handling
 *	receive()		read modem char or time out
 *	runPCPdial()		does a PCPursuit dial
 *	safeopen()		opens a file
 *	setRawDate()		set date (system dependent code)
 *	systemCommands()		run outside commands in the O.S.
 *	systemInit()		system dependent init
 *	systemShutdown()		system dependent shutdown
 *	WhatDay()		returns what day it is
 *
 *		# == local for this implementation only
 */

#define SETDISK		14

static char Refresh = 0;
static char UnderDesqView;

extern char *R_W_ANY;
extern char *READ_ANY;
extern char *READ_TEXT;
extern char *APPEND_TEXT;
extern char *APPEND_ANY;
extern char *A_C_TEXT;
extern char *WRITE_TEXT;
extern char *W_R_ANY;
extern char *WRITE_ANY;

/* Here's the rest of the goo */
void setTimer(TimePacket *Slast);
long milliTimeSince(TimePacket *Slast);
long timeSince(TimePacket *Slast);

SListBase ResList  = { NULL, ChkStrForElement, NULL, FreeNtoStr, ResIntrp };
SListBase BellList = { NULL, ChkTwoNumbers, NULL, free, EatTwoNumbers };
SListBase ChatBell = { NULL, ChkTwoNumbers, NULL, free, EatTwoNumbers };

char *ResFileName = "results.sys";

char *garp;

extern logBuffer logBuf;		/* Log buffer of a person       */
extern aRoom     roomBuf;
extern MessageBuffer      msgBuf;
extern CONFIG    cfg;		/* Lots an lots of variables    */
extern NetBuffer netBuf;
extern char onConsole;		/* Who's in control?!?		*/
extern char whichIO;		/* CONSOLE or MODEM		*/
extern char anyEcho;
extern char echo;
extern char modStat;
extern char echoChar;
extern char haveCarrier;
extern char outFlag;
extern char *strFile;
extern char *indexTable;
extern char loggedIn;

char straight = TRUE;
char oldmodem = FALSE;
void DVApiCall(int code);
int IsDesqView( void );
void OutString(char *s);

/*
 * Section 3.7. SYSTEM FORMATTING:
 *    These functions take care of formatting to strange places not
 * handled by normal C library functions.
 *   dPrintf() print to disk, using putMsgChar().
 *   mPrintf() print out the modem port via a mFormat() call.
 *   splitF() debug function, prints to both screen and disk.
 */

/*
 * dPrintf()
 *
 * This will write from format+args to disk, appends a null byte.
 */
void dPrintf(char *format, ...)
{
    va_list argptr;
    char garp[MAXWORD];

    va_start(argptr, format);
    vsprintf(garp, format, argptr);
    va_end(argptr);
    dLine(garp);
}

/*
 * mPrintf()
 *
 * This formats format+args to modem and console.
 *
 * NB: commands 118.662 - the stuff on recursion.  This function was NOT
 * re-entrant.  Doing a Help, using the paging feature, and hitting '?' at the
 * more prompt, would scramble stuff.
 */
int mPrintf(char *format, ...)
{
	va_list argptr;
	char localgarp[2000], *g;
	static int recursive = 0;

	if (garp == NULL || recursive) g = localgarp;
	else g = garp;
	recursive++;
	va_start(argptr, format);
	vsprintf(g, format, argptr);
	va_end(argptr);
	mFormat(g, oChar, doCR);
	recursive--;
	return 0;
}

/*
 * CitSystem()
 *
 * This function formats the format & arguments and then runs the result via
 * system().
 */
int CitSystem(char RestoreVideo, char *format, ...)
{
    va_list argptr;
    char localgarp[2000], *g;

    if (garp == NULL) g = localgarp;
    else g = garp;
    va_start(argptr, format);
    vsprintf(g, format, argptr);
    va_end(argptr);
    if (RestoreVideo) StopVideo();
    else printf("-%s-\n", g);
    system(g);
    if (RestoreVideo) VideoInit();
    return 0;
}

/*
 * printf()
 *
 * This formats format+args to console.
 */
int printf(const char *format, ...)
{
    va_list argptr;
    char    garp[2000];
    int     i;

    va_start(argptr, format);
    vsprintf(garp, format, argptr);
    va_end(argptr);
    if (straight) for (i = 0; garp[i]; i++) {
			if (garp[i] == '\n')
				DoBdos(6, '\r');
			DoBdos(6, garp[i]);
		}
    else	for (i = 0; garp[i]; i++) mputChar(garp[i]);
    return 0;
}

/*
 * NetPrintf()
 *
 * This formats format+args to a transmission function, adds a 0 byte on end.
 */
int NetPrintf(int (*method)(int c), char *format, ...)
{
    va_list argptr;
    char localgarp[2000], *g;
    int i;

    if (garp == NULL) g = localgarp;
    else g = garp;
    va_start(argptr, format);
    vsprintf(g, format, argptr);
    va_end(argptr);
    for (i = 0; g[i]; i++) {
	if (g[i] == '\n') g[i] = '\r';
	if (!(*method)(g[i])) return FALSE;
    }
    if (!(*method)(0)) return FALSE;      /* Send NULL since it did before */
    return TRUE;
}

/*
 * Section 3.8. TIMERS:
 *    Basically, the idea here is that two functions are available to
 * the rest of Citadel.  One starts a timer.  The other allows checking
 * that timer, to see how much time has passed since that timer was
 * started.  The remainder of the functions in this section are internal
 * to this implementation, mostly for use by receive().
 * 88Jun28: Now multiple timers accessible to Citadel are supported.
 */

static TimePacket localTimers[10];

/*
 * chkTimeSince()
 *
 * This is used to find out how much time has passed since initialization of
 * the given timer.
 * RETURNS: Time in seconds since last call to startTimer().
 */
long chkTimeSince(int TimerId)
{
    return timeSince(localTimers + TimerId);
}

/*
 * milliTimeSince()
 *
 * This will calculate how many milliseconds have passed.
 */
long milliTimeSince(TimePacket *Slast)
{
    long retVal;
    struct time timeblk;

    gettime(&timeblk);
    retVal = (timeblk.ti_sec != Slast->tPsecond) ? 100 : 0;
    retVal += timeblk.ti_hund - Slast->tPmilli;

    return retVal;
}

/*
 * pause()
 *
 * This function busy-waits N/100 seconds.
 */
void pause(int i)
{
    TimePacket x;
    long	(*fn)(TimePacket *r), limit;

    if (i == 0) return;
    fn = (i <= 99) ? milliTimeSince : timeSince;
    limit = (i <= 99) ? (long) i : (long) (i / 100);    /* Kludge */
    setTimer(&x);
	/* the reason for this Kludge is that Turbo C's delay() function */
	/* doesn't seem to be very reliable, at least not on this '386 box. */
    while ((*fn)(&x) <= limit) {
	if (cfg.DepData.IBM)
	    delay(i/2);
    }
}

/*
 * setTimer()
 *
 * This function intializes a timer.
 */
void setTimer(TimePacket *Slast)
{
    struct date dateblk;
    struct time timeblk;

    getdate(&dateblk);
    gettime(&timeblk);

    Slast->tPday     = (long) dateblk.da_day;
    Slast->tPhour    = (long) timeblk.ti_hour;
    Slast->tPminute  = (long) timeblk.ti_min;
    Slast->tPsecond  = (long) timeblk.ti_sec;
    Slast->tPmilli   = (long) timeblk.ti_hund;
}

/*
 * startTimer()
 *
 * This initializes a general timer.
 */
void startTimer(int TimerId)
{
    setTimer(localTimers + TimerId);
}

/*
 * timeSince()
 *
 * This function will calculate how many seconds have passed since "x".
 */
long timeSince(TimePacket *Slast)
{
    long retVal;
    struct date dateblk;
    struct time timeblk;

    getdate(&dateblk);
    gettime(&timeblk);

    retVal = (Slast->tPday == dateblk.da_day ? 0l : 86400l);
    retVal += ((timeblk.ti_hour - Slast->tPhour) * 3600);
    retVal += ((timeblk.ti_min - Slast->tPminute) * 60);
    retVal += (timeblk.ti_sec - Slast->tPsecond);
    return retVal;
}

/*
 *	Section 3.3 continued: Console stuff.
 */

char CurTime[10] = "";
/*
 * ScreenUser()
 *
 * This is called from givePrompt to display something when a room prompt
 * is displayed.
 */
void ScreenUser()
{
    if (cfg.DepData.OldVideo) {
	if (loggedIn) printf("(%s)\n", logBuf.lbname);
    }
    else if (Refresh) {
	Refresh = 0;
	StopVideo();
	VideoInit();
    }
}

/*
 * ScrTimeUpdate()
 *
 * This function updates the screen clock.
 */
void ScrTimeUpdate(int hr, int mn)
{
    char *civ;

    if (cfg.DepData.Clock == ALWAYS_CLOCK ||
       (cfg.DepData.Clock == BUSY_CLOCK && onLine())) {
	civTime(&hr, &civ);
	sprintf(CurTime, "%d:%02d %s", hr, mn, civ);
	ScrNewUser();
    }
}

char OnTime[20] = "";	/* not static so we can set it in sysdoor.c */
/*
 * ScrNewUser()
 *
 * This function is called when changes occur that might impact the status
 * line.
 */
void ScrNewUser()
{
	char  work[80];
	extern char CallSysop, ForceNet;
	extern SListBase ChatOn;

	if (!cfg.DepData.OldVideo) {
		if (onLine() && strLen(OnTime) == 0) {
			sprintf(OnTime, " %s", Current_Time());
		}
		else if (!onLine() && strLen(OnTime) != 0) {
			OnTime[0] = 0;
		}
		if (!onLine() && cfg.DepData.Clock == BUSY_CLOCK)
			CurTime[0] = 0;

		sprintf(work, "%-20s%-12s %c%-2s%2s%c%16s", logBuf.lbname, OnTime,
			(IsChatOn()) ?  'C' : ' ',
			CallSysop ?    "^T" : "  ",
			ForceNet ?     "^A" : "  ",
			!anyEcho ?      'E' : ' ', CurTime);
		statusline(work);
	}
}

/*
 * SpecialMessage()
 *
 * This will print a special* message on status line.
 */
void SpecialMessage(char *message)
{
    if (!cfg.DepData.OldVideo)
	statusline(message);
}

/*
 *	Section 3.9. MISCELLANEOUS.
 */

/*
 * diskSpaceLeft()
 *
 * This reveals the amount of space left on specified disk.  Internal work
 * function.
 */
void diskSpaceLeft(char drive, long *sectors, long *bytes)
{
    struct dfree dfreeblk;

    getdfree(toUpper(drive) - 'A' + 1, &dfreeblk);
    *bytes = (long) dfreeblk.df_avail * (long) dfreeblk.df_bsec *
					(long) dfreeblk.df_sclus;
    *sectors = ((*bytes) + 127) / SECTSIZE;
}

/*
 * getRawDate()
 *
 * This function gets the raw date from MSDOS.
 */
void getRawDate(int *year, int *month, int *day, int *hours, int *minutes,
						int *seconds, int *milli)
{
    struct date dateblk;
    struct time timeblk;

    getdate(&dateblk);
    gettime(&timeblk);

    *year  = dateblk.da_year;
    *month = dateblk.da_mon;
    *day  = dateblk.da_day ;
    *hours = timeblk.ti_hour;
    *minutes = timeblk.ti_min ;
    *seconds = timeblk.ti_sec ;
    *milli = timeblk.ti_hund;
}

/*
 * giveSpaceLeft()
 *
 * This will give the amount of space left on system.
 */
void GiveSpaceLeft(int thisRoom)
{
    long	sectors, bytes;
    extern char remoteSysop;

    char  dir[150], drive;

    if (FindDirName(thisRoom) != NULL) {
	strcpy(dir, FindDirName(thisRoom));
	MSDOSparse(dir, &drive);
	diskSpaceLeft(drive, &sectors, &bytes);
	if (!remoteSysop)
	    printf("\nThere are %s bytes left on drive %c:\n",
		PrintPretty(bytes, msgBuf.mbtext), drive);
	else
	    mPrintf("\nThere are %s bytes left on drive %c:\n ",
		PrintPretty(bytes, msgBuf.mbtext), drive);
    }
}

/*
 * nodie()
 */
int nodie()
{
    return 1;
}

/*
 * Control_C()
 *
 * This is a DOS handler for control C.
 */
int Control_C()
{
    Refresh++;
    return 1;
}

/*
 * receive()
 *
 * This gets a modem character, or times out ...
 * Returns:	char on success else ERROR.
 */
int receive(int seconds)
{
    TimePacket x;
    long   (*fn)(TimePacket *r), limit;

    if (!gotCarrier()) return ERROR;
    if (MIReady()) return Citinp();
    fn = (seconds == 1) ? milliTimeSince : timeSince;
    limit = (seconds == 1) ? 99l : (long) seconds;      /* Kludge */
    setTimer(&x);
    do {
	if (MIReady()) return Citinp();
    } while ((*fn)(&x) <= limit);
    return ERROR;
}

/*
 * safeopen()
 *
 * This function opens a file with some safeguards.
 */
FILE *safeopen(char *fn, char *mode)
{
    struct stat buff;

    if (stat(fn, &buff) == 0)
	if (buff.st_mode & S_IFCHR)
	    return NULL;

    return fopen(fn, mode);
}

/*
 * specCmpU()
 *
 * This is a special compare for this version's file tags.  I don't recall
 * why I need it, though.
 */
int specCmpU(char *f1, char *f2)
{
    while (toUpper(*f1) == toUpper(*f2)) f1++, f2++;
    if (*f1 == 0 && *f2 == ' ') return SAMESTRING;
    if (*f2 == 0) return -1;
    if (toUpper(*f1) < toUpper(*f2)) return -1;
    return 1;
}

/*
 * systemCommands()
 *
 * Some MS-DOS commands.
 */
void systemCommands()
{
    char filename[55], work[100];
    FILE *fd;
    char     *SysOpts[] = {
	"Delete file\n", "Outside commands\n", "X\beXit\n",
	" ",
	""
    };
    MenuId id;

    if (onConsole && cfg.Audit && cfg.BoolFlags.SysopEditor)
	ExtraOption(SysOpts, "View Calllog");

    id = RegisterSysopMenu("sysopt.mnu", SysOpts, " System Commands ", 0);

    while (onLine()) {
	outFlag = OUTOK;
	SysopMenuPrompt(id, "\n System commands: ");

	switch (GetSysopMenuChar(id)) {
	case ERROR:
	case 'X': CloseSysopMenu(id); return ;
	case 'D':
	    SysopRequestString(id, strFile, filename, sizeof filename, 0);
	    /* getNormStr(strFile, filename, sizeof filename, 0); */
	    /* doCR(); */
	    sprintf(work, "File %s.\n ", (unlink(filename) == 0) ?
					"deleted" : "not found");
	    SysopInfoReport(id, work);
	    break;
	case 'O':
	    CloseSysopMenu(id);
	    id = SysopContinual(" Patience ", "\n One moment, please", 25, 3);
	    writeSysTab();
	    if ((fd = safeopen(LOCKFILE, "w")) != NULL) {
fprintf(fd, 
"This is the Citadel-86 LOCK file, which is here to prevent you from\n"
"accidentally bringing up Citadel from within Citadel.  Do not delete\n"
"this file unless you are certain that you do not have Citadel up already.\n");
		fclose(fd);
	    }
	    SysopCloseContinual(id);
	    SysopRequestString(NO_MENU, "command line", filename, 100, 0);
	    if (whichIO == CONSOLE || strLen(filename) != 0) {
		clrscr();
		StopVideo();
		if (cfg.DepData.IBM) ModemShutdown(FALSE);
		system(filename);
		if (cfg.DepData.IBM) {
		    ModemOpen(FALSE);
		/*
		 * This is a kludge.  Since this is in a system dependency
		 * file, I'm not going to bother to really explain or feel
		 * guilty.
		 */
		    if (!gotCarrier()) DisableModem(FALSE);
		}
		homeSpace();
	    }
	    unlink(indexTable);
	    unlink(LOCKFILE);
	    if (!cfg.DepData.OldVideo && onConsole) {
		mPrintf("Any key.");
		getCh();
	    }
	    VideoInit();
	    id = RegisterSysopMenu("", SysOpts, " System Commands ", 0);
	    break;
	case 'V':
	    MakeCmdLine(work, cfg.DepData.Editor, "", sizeof work - 1);
	    makeAuditName(filename, "calllog.sys");
	    CloseSysopMenu(id);
	    if (cfg.DepData.IBM) ModemShutdown(FALSE);
	    CitSystem(TRUE, "%s %s", work, filename);
	    if (cfg.DepData.IBM) {
		ModemOpen(FALSE);
		if (!gotCarrier() && strLen(cfg.DepData.sDisable) == 0)
		    DisableModem(FALSE);
	    }
	    homeSpace();
	    id = RegisterSysopMenu("", SysOpts, " System Commands ", 0);
	    break;
	}
    }
}

/*
 * systemInit()
 *
 * This is the system dependent initialization routine.
 */
int systemInit()
{
    extern char locDisk, ourHomeSpace[100];
    SYS_FILE filename;
    static TwoNumbers TwoTemp = { 240, 100l };
    extern char AuditBase[];

    if ((garp = GetDynamic(8000)) == NULL)
	printf("WARNING: Couldn't allocate important buffer!\n");

    if (!CheckSystem())
	return 1;			/* error! */

    getcwd(ourHomeSpace, 99);

    locDisk = toUpper(ourHomeSpace[0]);

    if (cfg.Audit != 0) {
	/* ugly kludge */
	if (cfg.auditArea.saDisk == locDisk - 'A') {
	    strcpy(AuditBase, ourHomeSpace);
	}
	else {
	    DoBdos(SETDISK, cfg.auditArea.saDisk);
	    getcwd(AuditBase, 99);
	    DoBdos(SETDISK, locDisk - 'A');
	}

    /*
     * This grungy little kludge is possible because getcwd will return a
     * string of length 3 only when the cwd at the root of the disk, which
     * as it happens is precisely the only time we don't* want to append
     * a backslash.
     */
	if (strLen(AuditBase) != 3)
	    strcat(AuditBase, "\\");

	if (strLen(cfg.codeBuf + cfg.auditArea.saDirname) != 0) {
	    strcat(AuditBase, cfg.codeBuf + cfg.auditArea.saDirname);
	}
    }

    VideoInit();

    makeSysName(filename, "ctdlbell.sys", &cfg.roomArea);

    if (!MakeList(&BellList, filename, NULL))
	AddData(&BellList, &TwoTemp, NULL, FALSE);

    makeSysName(filename, "chatbell.sys", &cfg.roomArea);

    if (!MakeList(&ChatBell, filename, NULL))
	AddData(&ChatBell, &TwoTemp, NULL, FALSE);

    makeSysName(filename, ResFileName, &cfg.roomArea);
    MakeList(&ResList, filename, NULL);

    ArcInit();

#ifdef BREAK_READY
    if (cfg.DepData.IBM_or_clone) {
    printf("Setting setup_nocccb()\n");
	setup_nocccb();
    }
    else
	ctrlbrk(Control_C);
#else
    ctrlbrk(Control_C);
#endif

    if ((UnderDesqView = IsDesqView()))
	printf("DesqView detected.\n");

    InitProtocols();

    InitExternEditors();

    InitDoors();

    return 0;
}

/*
 * SystemDependentArgument()
 *
 * This function processes a candidate command line argument.  If recognized,
 * return TRUE, otherwise FALSE.
 */
int SystemDependentArgument(char *arg)
{
	if (strCmpU(arg, "-oldmodem") == 0) {
		oldmodem++;
		return TRUE;
	}

	return FALSE;
}

/*
 * CallChat()
 *
 * This function tries to attract the system operator's attention.
 */
void CallChat(int limit, int interruptable)
{
	int ring;

	for (ring = 0; ring < limit /* && gotCarrier() */; ring++) {
		RunListA(&ChatBell, BellIt, (interruptable) ? &interruptable :
								NULL);
		if (interruptable && BBSCharReady()) {
			modIn();
			return;
		}
		if (interruptable) {
			if (KBReady()) return;
			pause(300);
			if (KBReady()) return;
		}
	}
	/* if we get here reached the limit and will turn off chat flag */
	if (interruptable) {
		mPrintf("\n Sorry, Sysop not around...\n ");
		cfg.BoolFlags.noChat = TRUE;
	}
}

/*
 * ResIntrp()
 *
 * This function interprets a line from RESULTS.SYS and returns a structure
 * for use with result code processing.
 */
void *ResIntrp(char *line)
{
	char *mid;
	int rover;
	NumToString *temp;
	static struct {
		char *ResName;
		int  ResVal;
		int  Flags;
	} translate[] = {
		{ "#RESULT-300",       R_300, 0 },
		{ "#RESULT-1200",      R_1200, 0 },
		{ "#RESULT-2400",      R_2400, 0 },
		{ "#RESULT-4800",      R_4800, 0 },
		{ "#RESULT-9600",      R_9600, 0 },
		{ "#RESULT-14400",     R_14400, 0 },
		{ "#RESULT-19200",     R_19200, 0},
		{ "#RESULT-38400",     R_38400, 0},
		{ "#RESULT-56800",     R_56800, 0},
		{ "#RESULT-300-MNP",   R_300, MNP },
		{ "#RESULT-1200-MNP",  R_1200, MNP },
		{ "#RESULT-FAX",       R_FAX, 0 },
		{ "#RESULT-2400-MNP",  R_2400, MNP },
		{ "#RESULT-4800-MNP",  R_4800, MNP },
		{ "#RESULT-9600-MNP",  R_9600, MNP },
		{ "#RESULT-14400-MNP", R_14400, MNP },
		{ "#RESULT-19200-MNP", R_19200, MNP},
		{ "#RESULT-38400-MNP", R_38400, MNP},
		{ "#RESULT-56800-MNP", R_56800, MNP},
	        { "#RING",	       R_RING, 0 },
		{ "#DIALTONE",         R_DIAL, 0 },
		{ "#NO-DIALTONE",      R_NODIAL, 0 },
		{ "#OK",	       R_OK, 0 },
		{ "#NO-CARRIER",       R_NOCARR, 0 },
		{ "#BUSY",	       R_BUSY, 0 },
    };

	if ((mid = strchr(line, ' ')) != NULL) {
		*mid = 0;
		mid++;
		for (rover = 0; rover < NumElems(translate); rover++)
			if (strCmpU(line, translate[rover].ResName) ==
							SAMESTRING) {
				temp = (NumToString *) GetDynamic(sizeof *temp);
				temp->num  = translate[rover].ResVal;
				temp->num2 = translate[rover].ResVal;
				temp->string = strdup(mid);
				return temp;
			}
	}

	return NULL;
}

/*
 * ResultVal()
 *
 * This function tries to discover if the given result code is currently listed
 * and returns the appropriate symbolic value.
 */
int ResultVal(char *buf)
{
    NumToString *temp;

    temp = SearchList(&ResList, buf);
    if (temp != NULL)
	return temp->num;
    else
	return ERROR;
}

/*
 * VideoInit()
 *
 * This will initialize the video subsystem.
 */
void VideoInit()
{
    char work[60];
    extern char *VERSION;

    straight = FALSE;
    if (cfg.DepData.OldVideo) return;
    sprintf(work, "Citadel-86 V%s: ", VERSION);
    video(work);
    ScrNewUser();
}

/*
 * systemShutdown()
 *
 * This is the system dependent shutdown code.
 */
void systemShutdown(int SystemErrorValue)
{
    extern int exitValue;

    StopVideo();
}

/*
 * WhatDay()
 *
 * Returns what day it is (0=Sunday...).
 */
int WhatDay()
{
    _AX = 0x2a00;
    geninterrupt(0x21);
    return _AL;
}

/*
 * BeNice()
 *
 * This is used to be nice to the nice operating system.
 */
void BeNice(int x)
{
    if (UnderDesqView) {
	DVApiCall(0x1000);
	if (x == IDLE_PAUSE) {
	    DVApiCall(0x1000);
	    DVApiCall(0x1000);
	    DVApiCall(0x1000);
	}
    }
}

/*
 * IsDesqView()
 *
 * This function detects the presence of DesqView.
 */
int IsDesqView()
{
    union REGS s;

    s.x.cx = 0x4445;
    s.x.dx = 0x5351;
    s.x.ax = 0x2b01;
    intdos(&s, &s);
    return !(s.h.al == 0xff);
}

/*
 * DVApiCall()
 *
 * This is work code for making API calls to DesqView.
 */
void DVApiCall(int code)
{
    _BX = code;
    _AX = 0x101a;
    geninterrupt(0x15);
    _AX = _BX;
    geninterrupt(0x15);
    _AX = 0x1025;
    geninterrupt(0x15);
}

/*
 * SpaceBug()
 *
 * Grungy code for working around a bug in Turbo C.
 */
void SpaceBug(int x)
{
	char buf[100];

	if (x < sizeof buf) {
		setmem(buf, x, ' ');
		buf[x] = 0;
		mFormat(buf, oChar, doCR);
	}
}

/*
 * AbsToReadable()
 *
 * This will return a human string representing that date.
 */
char *AbsToReadable(unsigned long lastdate)
{
    struct tm   *data;
    char	*m;
    static char buffer[40];
    extern char *monthTab[];

    /* 0l represents never in our scheme */
    if (lastdate == 0l) return "Never";

    data = localtime((time_t *) &lastdate);
    civTime(&data->tm_hour, &m);

    sprintf(buffer, "%d%s%02d @ %d:%02d %s",
			data->tm_year, monthTab[data->tm_mon + 1],
			data->tm_mday, data->tm_hour, data->tm_min, m);
    return buffer;
}

/*
 * DialExternal()
 *
 * This function implements the external dialer.
 */
int DialExternal(NetBuffer *netBuf)
{
    if (cfg.DepData.IBM) ModemShutdown(FALSE);
    CitSystem(TRUE, "%s", netBuf->access);
    if (cfg.DepData.IBM) {
	ModemOpen(FALSE);
    }
    homeSpace();
    return gotCarrier();
}

/*
 * MoveToSysDirectory()
 *
 * This function handles moving to a system directory.
 */
void MoveToSysDirectory(SYS_AREA *area)
{
    SYS_FILE fn;

    sprintf(fn, "%c:%s", area->saDisk + 'a', cfg.codeBuf + area->saDirname);
    fn[strlen(fn) - 1] = 0;
    SetSpace(fn);
}

/*
 * SysArea()
 *
 * This function transforms the SYS_AREA to something in English and puts it
 * in buf.
 */
void SysArea(char *buf, SYS_AREA *area)
{
	extern char AuditBase[];

	if (&cfg.auditArea != area) {
		sprintf(buf,"%c:%s", area->saDisk + 'a',
						cfg.codeBuf + area->saDirname);
	}
	else strcpy(buf, AuditBase);
	buf[strlen(buf) - 1] = 0;
}

