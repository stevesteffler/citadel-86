/*
 *				misc.c
 *
 * Random functions.
 */

/*
 *				history
 *
 * 86Aug19 HAW  Kill history because of space problems.
 * 84Jun10 JLS  Function changedate() installed.
 * 84May01 HAW  Starting 1.50a upgrade.
 * 83Mar12 CrT  from msg.c
 * 83Mar03 CrT & SB   Various bug fixes...
 * 83Feb27 CrT  Save private mail for sender as well as recipient.
 * 83Feb23	Various.  transmitFile() won't drop first char on WC...
 * 82Dec06 CrT  2.00 release.
 * 82Nov05 CrT  Stream retrieval.  Handles messages longer than MAXTEXT.
 * 82Nov04 CrT  Revised disk format implemented.
 * 82Nov03 CrT  Individual history begun.  General cleanup.
 */

#include "ctdl.h"
#include "compress.h"

/*
 *				contents
 *
 *	ARCDir()		ARC TOC entries
 *	calcrc()		calculates CRC
 *	changeDate() 	   	allow changing of date
 *	CheckDLimit()	   	exceeded download time limit?
 *	CompressedDir()		manager of reading compressed dirs
 *	configure()		sets terminal parameters via dialogue
 *	crashout()		crashes out of Citadel in case of bug
 *	doFormatted()		for wildCard
 *	doCR()			newline on modem and console
 *	download()		menu-level routine for WC-protocol sends
 *	formRoom()		room prompt formatting
 *	getCdate()		gets date from system clock.
 *	GetSecond()		get seconds of minute, for multibanner
 *	GifDir()		important data of a GIF file.
 *	HelpIfPresent()		print help file if present
 *	ingestFile()		puts file in held message buffer
 *	lbyte()			finds 0 byte of a string
 *	patchDebug()		display/patch byte
 *	printDate()		prints out date	
 *	putBufChar()		.EWM/.EXM/.EWN/.EXN internal
 *	putFLChar()		readFile() -> disk file interface
 *	ReadMessageSpec()       analyze .r.. <string> thing.
 *	reconfigure()		Reconfigures a user
 *	TranFiles()		Handles file transfers to users
 *	TranSend()		Does send work of TranFiles()
 *	transmitFile()		send a host file, no formatting
 *	tutorial()		first level for printing a help file
 *	upLoad()		menu-level read-via-WC-protocol fn
 *	visible()		convert control chars to letters
 *	writeTutorial()		prints a .hlp file
 *	ZIPDir()		ZIP TOC entries
 */

#define	CREDIT_WARN	\
"WARNING: '%s' requires LD credits, and you do not have enough LD credits.\n " 

char *UploadLog;

FILE	*upfd;
int	masterCount;
int	acount;
UNS_32	BaudRate;	/* Bytes/sec that modem is set for.     */
int	DirAlign = 0;
char	AlignChar;
int     CurLine;
char    Pageable, Continuous;

char	*NoFileStr = "\n No %s.\n";
char	*who_str = "who";
char	*strFile = "filename";
/* char   *VERSION = "3.47.s6"; */
char	*VERSION = "3.49";
char	*ALL_LOCALS  = "&L";
char	*WRITE_LOCALS = "All Local Systems";
char	FormatFlag = FALSE;
long	Dl_Limit = -1l;
long	*DL_Total;   /* Blech */
char    more[15] = "More";

extern SListBase MailForward;

PROTO_TABLE Table[] = {
	{ "Ascii", 0, (IS_NUMEROUS | NEEDS_HDR), "ASCII", NULL, NULL, NULL,
		/* sendAscii */ outMod, 1, AsciiHeader, NULL },
	{ "Xmodem", 13, (RIGAMAROLE | IS_DL), "Xmodem", "Xmodem",
		"wcdown.blb", "wcupload.blb", sendWCChar, SECTSIZE, NULL,
								XYClear },
	{ "Ymodem", 11, (IS_NUMEROUS | RIGAMAROLE | IS_DL | NEEDS_FIN | NEEDS_HDR),
		"Ymodem BATCH", "Ymodem SINGLE", "ymdown.blb", "ymodemup.blb",
		sendYMChar, YM_BLOCK_SIZE, YMHdr, XYClear },
#ifdef WXMODEM_AVAILABLE
	{ "Wxmodem", 13, (RIGAMAROLE | IS_DL), "WXModem",
		"WXModem", "wxdown.blb", "wxup.blb", sendWXModem, SECTSIZE,
		NULL, ClearWX }
#else
	{ NULL, 13, NOT_AVAILABLE, NULL, NULL, NULL, NULL, NULL, 0, 
		NULL, NULL }
#endif
} ;

int fixVers = 699;
int majorVers = 120;
static char IgnoredShown;
char *netVersion = "1.18";

#define AUDIT   9000
char	audit[AUDIT];

extern CONFIG	   cfg;		/* Lots an lots of variables    */
extern logBuffer   logBuf;		/* Person buffer		*/
extern logBuffer   logTmp;		/* Person buffer		*/
extern aRoom	   roomBuf;		/* Room buffer		*/
extern rTable	   *roomTab;
extern MessageBuffer     msgBuf;
extern MessageBuffer     tempMess;
extern NetBuffer   netBuf;
extern int	   outPut;
extern char	   onConsole;
extern AN_UNSIGNED crtColumn;	/* where are we on screen now?	*/
extern char	   loggedIn;	/* Is we logged in?			*/
extern char	   outFlag;	   /* Output flag			*/
extern char	   haveCarrier;    /* Do we still got carrier?     */
extern char	   heldMess;
extern int	   TransProtocol;  /* transfer protocol in use     */
extern char	   prevChar;	/* previous char output		*/
extern char	   *READ_ANY, *READ_TEXT;
extern char	   textDownload;   /* flag	   */
extern int	   thisRoom;
extern int	   thisLog;
extern char	   whichIO;	   /* Where I/O is	   */
extern char	   echo;	   /* Should we echo? echo? echo?  */
extern FILE	   *msgfl;
extern FILE	   *roomfl;
extern FILE	   *logfl;
extern int	   exitValue;
extern char	   *LCHeld, *WRITE_ANY, *WRITE_TEXT;
extern char	   PrintBanner;
extern char	   *R_SH_MARK, *LOC_NET, *NON_LOC_NET;

FunnyInfo Formats[] = {
	{ "LZH", TRUE,  LZHDir },
	{ "ZIP", TRUE,  ZIPDir },
	{ "ZOO", TRUE,  ZOODir },
	{ "ARC", TRUE,  ARCDir },
	{ "GIF", FALSE, GifDir },
	{ "FRA", FALSE, GifDir },
	{ NULL,  FALSE, NULL }
};

#define FMSGS		"msgs.cit"

/*
 * CompressType()
 *
 * This function finds the type of file the specified file is.
 */
int CompressType(char *name)
{
    int format;
    char *c;

    if ((c = strchr(name, '.')) != NULL) {
	for (format = 0; Formats[format].Format != NULL; format++)
	    if (strCmpU(c + 1, Formats[format].Format) == SAMESTRING)
		return format;
    }
    return -1;
}

/*
 * AsciiHeader()
 *
 * This will entitle an ASCII file transfer.
 */
int AsciiHeader(long fileSize, char *filename)
{
    char work[10];

    doCR();
    mPrintf("[ %s : %s bytes ]", filename, PrintPretty(fileSize, work));
    doCR();
    doCR();
    return TRUE;
}

#define FN_LENGTH	90
/*
 * CompressedDir()
 *
 * This function reads the TOC of compressed files using a table of function
 * pointers (for generic use) and displays it.
 */
void CompressedDir(DirEntry *fn)
{
    FILE	*fd;
    char	FileName[FN_LENGTH];
    char	DateStr[20];
    long	RealSize, SmallSize;
    int		count = 0;
	long	compressed = 0l, realsize = 0l;
	int		format;
	extern int DirAlign;
	extern char AlignChar;

	if (outFlag != OUTOK) return;

	if ((format = CompressType(fn->unambig)) == ERROR) {
		ReadExternalDir(fn->unambig);
		return;
	}

	PagingOn();

	mPrintf("\n %s", fn->unambig);
	if (FindFileComment(fn->unambig, FALSE)) {
		DirAlign = strLen(fn->unambig) + 3;
		AlignChar = 0;
		mPrintf(":%s", strchr(msgBuf.mbtext, ' '));
		DirAlign = 0;
	}

	mPrintf("\n ");
	if ((fd = fopen(fn->unambig, READ_ANY)) == NULL) {
		mPrintf("INTERNAL FILE ERROR!\n ");
		PagingOff();
		return ;
	}
	if (Formats[format].Many) {
		mPrintf("\n %-15s %7s%8s date\n ","Name", "Crunched", "Normal");
		while ((*Formats[format].Func)(fd, FileName, &RealSize,
							&SmallSize, DateStr)) {
			count++;
			mPrintf("\n %-15s %7ld%8ld %s", FileName, SmallSize,
							RealSize, DateStr);
			compressed += SmallSize;
			realsize   += RealSize;
		}
		mPrintf("\n %-16s------- -------", "");
		mPrintf("\n %5d %-10s%7ld%8ld\n ", count, "files",
							compressed, realsize);
	}
	else {
		(*Formats[format].Func)(fd, TRUE, msgBuf.mbtext);
		mPrintf("%s", msgBuf.mbtext);
	}

	fclose(fd);
	PagingOff();
}

/*
 * ARCDir()
 *
 * This function reads an ARC TOC entry and sets for the next one.
 */
char ARCDir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr)
{
    ARCbuf buf;

#ifndef IS_MOTOROLA
    if (fread(&buf, sizeof buf, 1, fd) <= 0) 
	return FALSE;
#else
    /* this mess is due to Lattice C doing structure padding on Amigas */
    fread(&buf.ArchiveMark, 1, 1, fd);
    fread(&buf.Header, 1, 1, fd);
    fread(buf.name, 13, 1, fd);
    fread(&buf.size, 4, 1, fd);
    fread(&buf.date, 2, 1, fd);
    fread(&buf.time, 2, 1, fd);
    fread(&buf.crc, 2, 1, fd);
    fread(&buf.length, 4, 1, fd);
#endif

    if (buf.ArchiveMark != 0x1a || buf.Header == 0)
	return FALSE;

    strcpy(FileName, buf.name);
#ifdef IS_MOTOROLA
    Intel32ToMotorola(&buf.size);
    Intel32ToMotorola(&buf.length);
    Intel16ToMotorola(&buf.date);
#endif
    *SSize = buf.size;
    *RSize = buf.length;

    DosToNormal(DateStr, buf.date);

    fseek(fd, buf.size, SEEK_CUR);

    return TRUE;
}

/*
 * ZIPDir()
 *
 * This function reads a ZIP TOC entry and sets for the next one.
 */
char ZIPDir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr)
{
    ZipHeader ZBuf;

    if (fread(&ZBuf, sizeof ZBuf, 1, fd) < 1) return FALSE;

#ifdef IS_MOTOROLA
    Intel32ToMotorola(&ZBuf.Signature);
    Intel32ToMotorola(&ZBuf.CompSize);
    Intel32ToMotorola(&ZBuf.NormalSize);
    Intel16ToMotorola(&ZBuf.NameLength);
    Intel16ToMotorola(&ZBuf.FieldLength);
    Intel16ToMotorola(&ZBuf.FileDate);
#endif

    if (ZBuf.Signature != 0x04034b50) return FALSE;

    if (ZBuf.NameLength < 0 || ZBuf.NameLength > FN_LENGTH) return FALSE;

    fread(FileName, ZBuf.NameLength, 1, fd);

    FileName[ZBuf.NameLength] = 0;

    fseek(fd, ZBuf.FieldLength + ZBuf.CompSize, 1);

    *SSize = ZBuf.CompSize;
    *RSize = ZBuf.NormalSize;

    DosToNormal(DateStr, ZBuf.FileDate);

    return TRUE;
}

/*
 * GifDir()
 *
 * This reads the important data of a GIF file.
 */
char GifDir(FILE *fd, char longexpl, char *sbuf)
{
    GifHeader buf;

    fread(&buf, sizeof buf, 1, fd);

#ifdef IS_MOTOROLA
    Intel16ToMotorola(&buf.Width);
    Intel16ToMotorola(&buf.Height);
#endif

    sbuf[0] = 0;

    if (longexpl)
	sprintf(sbuf, "File is %.6s, ", buf.Sig);

    sprintf(lbyte(sbuf), (longexpl) ? "%d X %d, %d colors.\n " :
			 "%3d X %3d X %2d", buf.Width, buf.Height,
					1 << ((buf.Colors & 0x07) + 1));
    return TRUE;
}

/*
 * ZOODir()
 *
 * This handles reading a Zoo entry.
 */
char ZOODir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr)
{
	zoo_header zh;
	zoo_direntry de;
	static char ZooStart = TRUE;

	if (ZooStart) {
		ZooStart = FALSE;
		fread(&zh, sizeof zh, 1, fd);
#ifdef IS_MOTOROLA
		Intel32ToMotorola(&zh.zoo_start);
#endif
		fseek(fd, zh.zoo_start, 0);
	}

	do {
		if (fread(&de, sizeof de, 1, fd) < 1) {
			ZooStart = TRUE;
			return FALSE;
		}
#ifdef IS_MOTOROLA
		Intel32ToMotorola(&de.next);
#endif
		fseek(fd, de.next, 0);
	} while (de.deleted);

#ifdef IS_MOTOROLA
	Intel32ToMotorola(&de.size_now);
	Intel32ToMotorola(&de.org_size);
	Intel16ToMotorola(&de.date);
#endif
	*SSize = de.size_now;
	*RSize = de.org_size;
	strcpy(FileName, de.fname);

	DosToNormal(DateStr, de.date);

	return TRUE;
}

/*
 * LZHDir()
 *
 * This function handles reading a lzh entry.
 *
 * courtesy Daniel Durbin.
 */
char LZHDir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr)
{
    LZHead header;

#ifndef IS_MOTOROLA
    if (fread(&header, sizeof(header), 1, fd) < 1)
	return FALSE;
#else
    if (fread(header.unknown1, sizeof header.unknown1, 1, fd) < 1)
	return FALSE;
    if (fread(header.method, sizeof header.method, 1, fd) < 1)
	return FALSE;
    if (fread(&header.csize, sizeof header.csize, 1, fd) < 1)
	return FALSE;
    if (fread(&header.fsize, sizeof header.fsize, 1, fd) < 1)
	return FALSE;
    if (fread(&header.ftime, sizeof header.ftime, 1, fd) < 1)
	return FALSE;
    if (fread(&header.fdate, sizeof header.fdate, 1, fd) < 1)
	return FALSE;
    if (fread(&header.fattr, sizeof header.fattr, 1, fd) < 1)
	return FALSE;
    if (fread(&header.unknown2, sizeof header.unknown2, 1, fd) < 1)
	return FALSE;
    if (fread(&header.namelen, sizeof header.namelen, 1, fd) < 1)
	return FALSE;
#endif

    if (header.namelen < 1 || header.namelen >= FN_LENGTH)
	return FALSE;

    if (fread(FileName, 1, header.namelen, fd) != header.namelen)
	return FALSE;

    FileName[header.namelen] = 0;
    fgetc(fd), fgetc(fd);	/* gets CRC I guess */

    /* I don't* know what this is jumping over - but it works! */
    if (header.unknown2 == 1) {
	fgetc(fd);
	fgetc(fd);
	fgetc(fd);
    }

#ifdef IS_MOTOROLA
    Intel32ToMotorola(&header.fsize);
    Intel32ToMotorola(&header.csize);
    Intel16ToMotorola(&header.fdate);
#endif

    *SSize = header.csize;
    *RSize = header.fsize;
    DosToNormal(DateStr, header.fdate);

    fseek(fd, header.csize, SEEK_CUR);
    return TRUE;
}

/*
 * DosToNormal()
 *
 * This function converts a DOS-formatted date to a formatted string.  This
 * perhaps should reside in the system dependent code....
 */
void DosToNormal(char *DateStr, UNS_16 DosDate)
{
    extern char *monthTab[13];

    if (((DosDate & 0x1e0) >> 5) > 12 ||
		((DosDate & 0x1e0) >> 5) < 1)
	strcpy(DateStr, "No Date");
    else
	sprintf(DateStr, "%d%s%02d", ((DosDate & 0xfe00) >> 9) + 80,
			monthTab[(DosDate & 0x1e0) >> 5], DosDate & 0x1f);
}

/*
 * calcrc()
 *
 * Calculates CRC for a given block.
 */
CRC_TYPE calcrc(unsigned char *ptr, int count)
{
  register CRC_TYPE checksum;
  register int i;

  checksum=0;

    while (count--)
    {
	i=(checksum >> 8) & 0xff;
	i ^= *ptr++;
	i ^= i >> 4;
	checksum <<= 8;
	checksum ^= i;
	i <<= 5;
	checksum ^= i;
	i <<= 7;
	checksum ^= i;
    }
  return(checksum);
}

#ifdef NEED_AVAILABLE
/************************************************************************/
/*	changedate() gets the date from the aide and remembers it	*/
/************************************************************************/
void changeDate()
{
    int year, day, hours, minutes, mon;
    char *month;

    mPrintf("Current date is: %s\n ", formDate());
    getCdate(&year, &month, &day, &hours, &minutes);
    mPrintf("Current time is: %d:%02d\n ", hours, minutes);
    if (!getYesNo("Enter a new date & time"))
	return ;

    do {
	year    = (int) getNumber("Year",  0l, 9999l);
	mon     = (int) getNumber("Month", 1l,  12l)	   ;
	day     = (int) getNumber("Day",   1l,  31l)	   ;
	hours   = (int) getNumber("Hour",   0l, 23l)	   ;
	minutes = (int) getNumber("Minute", 0l, 59l)	   ;
    } while (!setRawDate(year, mon, day, hours, minutes));
/*    InitEvents(); */
}
#endif

/*
 * CheckDLimit()
 *
 * This checks to see if the next d/l will exceed the the limit or if it'll
 * interfere with a preemptive event.  It returns FALSE on interference,
 * TRUE otherwise.
 */
char CheckDLimit(long estimated)
{
    char *problem;
    extern char *DlMsgPtr;

    if (!aide && Dl_Limit_On() &&
			(*DL_Total) + estimated >= Dl_Limit * 60) {
	mPrintf("I'm sorry, that would exceed the current cumulative download time limit ");
	if (strLen(DlMsgPtr) != 0)
	    mPrintf("of %s", DlMsgPtr);
	mPrintf(" -- you've currently spent %ld:%02ld in downloading.\n ",
				(*DL_Total) / 60l, (*DL_Total) % 60l);
	return FALSE;
    }
    if ((problem = ChkPreempt(estimated)) != NULL) {
	mPrintf("Sorry, that would interfere with %s.\n ", problem);
	return FALSE;
    }
    return TRUE;
}

/*
 * configure()
 *
 * This sets up the terminal width etc via dialogue.
 */
char configure(logBuffer *lBuf, char AllQuestions, char AllowAbort)
{
    extern char *AbortAcct;
    int width, xwidth;		/* really! ugly kludge -- fix someday */

    lBuf->lbnulls   = 0;
    lBuf->lbdelay  = 0;
    width = termWidth;
    do {	/* this gross width stuff is caused by that #define in ctdl.h */
	termWidth = width;
	lBuf->lbwidth   = (int) getNumber("screen width in columns", 0l, 255l);
	xwidth = lBuf->lbwidth;
	if (onLine() && lBuf->lbwidth == 0 && AllowAbort) {
	    termWidth = width;
	    if (getYesNo(AbortAcct))
		return FALSE;
	}
	if (lBuf->lbwidth < 10) {
	    termWidth = width;
	    mPrintf("Sorry, must be at least 10\n");
	}
	lBuf->lbwidth = xwidth;
    } while (onLine() && lBuf->lbwidth < 10);
    if (AllQuestions) {
	lBuf->lbflags.LFMASK = getYesNo(" Do you need Linefeeds") ? TRUE : FALSE;
    }
    else {
	mPrintf("XXXXXX");
	doCR();
	mPrintf("XXXXXX");
	doCR();
	lBuf->lbflags.LFMASK = getYesNo(" Is there a blank line between the Xs") ? FALSE : TRUE;
    }
    lBuf->lbflags.EXPERT      = getYesNo(" Are you an experienced Citadel user")
							? TRUE : FALSE;
    if (lBuf->lbflags.EXPERT || AllQuestions) {
	lBuf->lbflags.TIME =
		getYesNo(" Print time messages created") ? TRUE : FALSE;
	lBuf->lbflags.OLDTOO   =
		getYesNo(" Print last Old message on <N>ew Message request")
							? TRUE : FALSE;
	lBuf->lbflags.FLOORS = getYesNo(" Floor mode");
    }
    else {
	lBuf->lbflags.OLDTOO = FALSE;
	lBuf->lbflags.TIME = TRUE;
	lBuf->lbflags.FLOORS = lBuf->lbflags.HALF_DUP = FALSE;
    }
    if (AllQuestions) {
	lBuf->lbpage = (int) getNumber("page length (0 to disable)", 0l, 255l);
	logBuf.lbflags.MSGPAGE = getYesNo("Do Message Paging");
    }
    return TRUE;
}

/*
 * crashout()
 *
 * Problems?  Out we go!!! This is a general error exit function.
 */
void crashout(char *message)
{
    FILE *fd;				/* Record some crash data */
    int  i;

    exitValue = CRASH_EXIT;
    outFlag = IMPERVIOUS;
   mPrintf("\n Whoops!! CRASHOLA!! Thanks and bye, leave mail on Test Sys!\n ");
    printf("ERROR: -%s-\n", message);
    HangUp(FALSE);
    logMessage(L_OUT, 0l, 0);
    logMessage(CRASH_OUT, 0l, 0);
    fd = fopen("crash", "w");
    fprintf(fd, message);
    fclose(fd);
    fd = fopen("audit", "w");
    for (i = 0; i < AUDIT; i++) {
	fputc(audit[i], fd);
	if ((i+1) % 70 == 0) fprintf(fd, "\n");
    }
    fprintf(fd, "\n\ncounter = %d\n", acount);
    fclose(fd);
    writeSysTab();
    ModemShutdown(TRUE);
    systemShutdown(0);
    exit(exitValue);
}

/*
 * doFormatted()
 *
 * This does a tutorial for a wildCard call.
 */
void doFormatted(DirEntry *fn)
{
	char     line[MAXWORD];
	FILE *fbuf;

	if ((fbuf = safeopen(fn->unambig, READ_TEXT)) == NULL) {
		mPrintf(NoFileStr, fn->unambig);
		return ;
	}

	if (!expert) mPrintf("\n <J>ump <P>ause <S>top\n");
/*	mPrintf(" \n");	*/
	doCR();
	while (fgets(line, MAXWORD, fbuf) && outFlag != OUTSKIP)
		mPrintf("%s", line);

	fclose(fbuf);
}

/*
 * doCR()
 *
 * This does a newline on modem and console.
 */
char doCR()
{
	int i;

	crtColumn   = 1;
	if (outFlag != OUTOK &&     /* output is being s(kip)ped    */
		outFlag != OUTPARAGRAPH && outFlag != IMPERVIOUS &&
					outFlag != NO_CANCEL)
		return FALSE;
	++CurLine;

	if (outPut == DISK) fprintf(upfd, "\n");
	else {
		ShoveCR();
		if (!MoreWork(FALSE)) return FALSE;

			/* Kludge alert!  Kludge alert! */
			/* We don't have to check TransProtocol, though. */
		if (DirAlign != 0 && termWidth > 22) {
#ifndef TURBO_C_VSPRINTF_BUG
			mPrintf("%*c%c ", DirAlign, ' ', AlignChar);
#else
	/* SUPER YUCKY! */
			crtColumn += DirAlign + 1;
			for (i = 0; i < DirAlign; i++) {
				mputChar(' ');
				if (haveCarrier)
					(*Table[TransProtocol].method)(' ');
			}
			mputChar(AlignChar);
			mputChar(' ');
			if (haveCarrier) {
				(*Table[TransProtocol].method)(AlignChar);
				(*Table[TransProtocol].method)(' ');
	    }
#endif
		}
	}
	prevChar    = ' ';
	return TRUE;
}

/*
 * Fix 117.653 - user has a width less than the sysop settable More prompt.
 * Problem was we were recursing (MoreWork -> AndMoreWork -> doCR -> putWord ->
 * MoreWork) to infinity.  Now we short-circuit any recursion.
 */
char MoreWorkRecursion;

/*
 * MoreWork()
 *
 * This function does the more work.
 */
char MoreWork(char AtMsg)
{
	extern char	inNet;
	extern char Showing, ManualMsgPage;
	int result;

	if (MoreWorkRecursion) {
		return TRUE;
	}

	if (Continuous) {
		return TRUE;
	}

	if (outPut == NORMAL && TransProtocol== ASCII &&
				outFlag != OUTSKIP && outFlag != OUTNEXT) {
		if (inNet == NON_NET && Pageable &&
		((logBuf.lbpage != 0 && CurLine>=logBuf.lbpage) ||
		 (AtMsg && Showing == MSGS &&
				(ManualMsgPage ^ logBuf.lbflags.MSGPAGE)))) {
			MoreWorkRecursion++;
			if (AtMsg) ShoveCR();
			result = AndMoreWork(2);
			MoreWorkRecursion--;
			crtColumn   = 1;
			if (result == MORE_ONE) {
				CurLine--;
				return ERROR;
			}
	  
			/*
			 * 2? you say?  Well ... it turns out it looks better
			 * during paging, at least for message display.  I'm
			 * not quite sure why.
			 */
			CurLine = 2;
			if (result == MORE_FLOW_CONTROL &&
						outFlag != NO_CANCEL) {
				outFlag = OUTSKIP;
				return FALSE;
			}
		}
	}
	return TRUE;
}

/*
 * AndMoreWork()
 *
 * Actually print more ...
 */
int AndMoreWork(int debug)
{
	char More[20];
	int  result, i;

	if (!onLine()) return FALSE;
	do {
		sprintf(More, "<%s>", more);
		putWord(More, oChar, doCR);
		result = HandleControl(TRUE);
	} while (result == MORE_UNDECIDED);
	for (i = 0; i < strlen(More); i++) oChar('\b');
	return result;
}

/*
 * ShoveCR()
 *
 * Do the actual shoving of a carriage return.
 */
void ShoveCR()
{
    int i;

    if (TransProtocol == ASCII)
	mputChar(NEWLINE);

    if (haveCarrier) {
	(*Table[TransProtocol].method)('\r');
	if (TransProtocol == ASCII)
	    for (i = termNulls;  i;  i--) outMod(0);
	if (termLF)
	    (*Table[TransProtocol].method)('\n');
    }
}

/*
 * download()
 *
 * This is the is the menu-level send-message-via-protocol function.
 */
void download(int msgflags, char protocol, char global, int Compression,
					OfflineReader *Reader)
{
	char CollectOffline = 0;
	MSG_NUMBER	*msgptrs = NULL;
	char result;
	int  count, i;
	extern char *APPEND_TEXT, Showing, *Basename;
	char CompFile[30];

	outFlag     = OUTOK;

	if (protocol != ASCII || Reader != NULL) {
		mPrintf("This transfer of messages will use ");
		if (Reader != NULL)
			mPrintf("the %s reader ", Reader->Name);
		if (!InternalProtocol(protocol) ||
				Table[protocol].MsgTran != NULL)
			mPrintf("%s protocol",
			InternalProtocol(protocol) ? Table[protocol].MsgTran :
						FindProtoName(protocol));
		if (Compression != NO_COMP)
			mPrintf(", using %s", GetCompEnglish(Compression));
		mPrintf(".  ");
	}

	if (InternalProtocol(protocol) && !expert &&
					Table[protocol].BlbName != NULL)
		printHelp(Table[protocol].BlbName, HELP_SHORT);

	if (		Reader == NULL &&
			InternalProtocol(protocol) &&
			Compression == NO_COMP &&
			protocol != ASCII) {
		if (!getYesNo("Ready"))  return;
	}

	if (!InternalProtocol(protocol) || Compression != NO_COMP ||
							Reader != NULL) {
		CollectOffline++;
		result = TRAN_SUCCESS;
		ToTempArea();
		mPrintf("There will be a delay while messages are collected...\n ");
		if (Reader == NULL) {
			echo = NEITHER;
			if (!redirect(FMSGS, INPLACE_OF)) return;
		}
		else {
			if ((upfd = fopen(FMSGS, WRITE_ANY)) == NULL) {
				mPrintf("Could not create temp file, aborting.\n ");
				return;
			}
		}
	}
	else {
		if (protocol != ASCII) echo = NEITHER;
		result = Transmission(protocol, STARTUP);
	}

	if (Reader != NULL || protocol != ASCII || Compression != NO_COMP)
		Showing = DL_MSGS;

	if (result == TRAN_SUCCESS) {
		if (!global) {
			PrepareForMessageDisplay(msgflags & REV);
			count = showMessages(PAGEABLE|msgflags,
						logBuf.lastvisit[thisRoom],
						Reader==NULL ? OptionValidate :
							ORWriteMsg);
	    		if (!expert && count == 0)
				mPrintf("\n \n There are no new messages in this room.\n ");
		}
		else {
			msgptrs = (MSG_NUMBER *)
				GetDynamic(MAXROOMS * sizeof (MSG_NUMBER));
			for (i = 0; i < MAXROOMS;  i++)
				msgptrs[i]  = logBuf.lastvisit[i];
			doGlobal(msgflags | ((Reader == NULL) ? TALK : NO_TALK),
				(Reader == NULL) ? OptionValidate : ORWriteMsg);
		}

		/*
		 * commands 119.665 - close now before running the offline
		 * reader command.
		 */
		if (CollectOffline) {
			if (Reader == NULL) undirect();
			else fclose(upfd);
		}

		if (Reader != NULL) {
			homeSpace();
			DropFile(0, TDirBuffer);
			CitSystem(TRUE, "%s", Reader->Cmd);
			homeSpace();
			if (!OR_Result(CompFile)) {
				RmTempFiles(TDirBuffer);
				RestorePointers(msgptrs);
				return;
			}
			if (chdir(TDirBuffer) != 0) {
				mPrintf("Offline reader internal error\n ");
				RestorePointers(msgptrs);
				return;
			}
		}

		if (!InternalProtocol(protocol) || Compression != NO_COMP) {
			if (Compression != NO_COMP) {
				mPrintf("Compressing messages...\n ");
				if (Reader == NULL)
					sprintf(CompFile, "%s.%s", Basename,
						CompExtension(Compression));
				Compress(Compression, ALL_FILES, CompFile);
				unlink(FMSGS);	/* because we use TranSend to send files */
				if (access(CompFile, 0) != 0) {
					mPrintf("Error: The compression failed.\n ");
					RmTempFiles(TDirBuffer);
					KillTempArea();
					RestorePointers(msgptrs);
					return;
				}
			}
			else strcpy(CompFile, FMSGS);

			if (getYesNo("Message collection done, ready"))
				TranSend(protocol, transmitFile, CompFile, "", FALSE);
			else RestorePointers(msgptrs);

			RmTempFiles(TDirBuffer);
			unlink(UploadLog);
			KillTempArea();
 		}
		else if (Transmission(TransProtocol, FINISH) != TRAN_SUCCESS)
			RestorePointers(msgptrs);
		Showing = WHATEVER;
	}

	echo = BOTH;
	TransProtocol = ASCII;

	/*
	 * If we have a console timeout during message display (during a Pause,
	 * most likely), onLine() will not be true at this point.  But setUp()
	 * will blindly set it to TRUE, so we have to call this with some care.
	 */
	if (onLine())
		setUp(FALSE);
}

/*
 * RestorePointers()
 *
 * This function restores msg pointers.
 */
static void RestorePointers(MSG_NUMBER	*msgptrs)
{
	int i;

	if (msgptrs != NULL) {
		for (i = 0; i < MAXROOMS;  i++)
			logBuf.lastvisit[i] = msgptrs[i];
		free(msgptrs);
	}
}
/*
 * RmTempFiles()
 *
 * This function deletes all files in the specified directory, and then
 * torches the directory.
 */
void RmTempFiles(char *dir)
{
	if (chdir(dir) == 0) {
		wildCard(DelFile, ALL_FILES, "", WC_NO_COMMENTS);
		homeSpace();
	}
	KillTempArea();
}

int StartingRoom, CurRoom;
/*
 * doGlobal()
 *
 * Does .R{Y,W,X,other protocols}G
 */
static void doGlobal(int flags, ValidateShowMsg_f_t * func)
{
	extern char PhraseUser;
	int gflags;

	if (outPut == NORMAL) flags |= PAGEABLE | MSG_LEAVE_PAGEABLE;

	StartingRoom = CurRoom = thisRoom;
	gflags = MOVE_GOTO | ((flags & TALK) ? MOVE_TALK : 0);

	while (
		((READMSG_TYPE(flags) == NEWoNLY &&
			!PhraseUser) ? gotoRoom("", gflags) : NextSeq())
				&& (gotCarrier() || onConsole)) {
		if ((flags & TALK) && outPut == DISK) {
			outPut = NORMAL;
			mPrintf("Working on %s\n ", roomBuf.rbname);
			outPut = DISK;
		}
		if (flags & TALK) givePrompt();
		if (flags & TALK) mPrintf("read\n ");
		PrepareForMessageDisplay(flags & REV);
		showMessages(flags & ~(TALK|NO_TALK),
					logBuf.lastvisit[thisRoom], func);
		if (flags & TALK) doCR();	/* aesthetics, pig-dogs. */
		if (outFlag == OUTSKIP) break;
	}
	PagingOff();
}

/*
 * PrepareForMessageDisplay()
 *
 * This function is called in preparation for reading a room for a user.
 * This consists currently of setting the StartSlot variable of Opt
 * given what room we're in.
 */
static void PrepareForMessageDisplay(char revOrder)
{
	int              rover, GoodMessages;
	MSG_NUMBER       msgNo;
	int              start, finish, increment;
	extern OptValues Opt;

	if (Opt.MaxMessagesToShow <= 0)
		return;

	/*
	 * OK, find the point in the room slots where we want to start,
	 * or stop, depending on how we are doing this, showing messages.
	 */
	SetShowLimits(!revOrder, &start, &finish, &increment);

	for (rover = start, Opt.StartSlot = 0, GoodMessages = 0;
	     rover != finish && GoodMessages < Opt.MaxMessagesToShow;
	     rover += increment)
	{
		msgNo = (roomBuf.msg[rover].rbmsgNo & S_MSG_MASK);
	        if (msgNo > cfg.oldest)
		{
			Opt.StartSlot = rover;
			GoodMessages++;
		}
	}
}

/*
 * NextSeq()
 *
 * This finds next room in sequence for doGlobal().
 */
int NextSeq()
{
    int i;

    i = (CurRoom + 1) % MAXROOMS;
    while (i != StartingRoom) {
	if (roomTab[i].rtflags.INUSE &&
		KnownRoom(i) != UNKNOWN_ROOM) {
	    getRoom(i);
	    CurRoom = i;
	    return TRUE;
	}
	i = (i + 1) % MAXROOMS;
    }
    return FALSE;
}

/*
 * GetSecond()
 *
 * This will return the second of the minute.  For multibanner.
 */
int GetSecond()
{
    int y, d, h, m, seconds, ml, mon;

    getRawDate(&y, &mon, &d, &h, &m, &seconds, &ml);
    return seconds;
}

/*
 * ingestFile()
 *
 * This puts the given file in specified buffer.
 */
char ingestFile(char *name, char *mbtext)
{
    char  filename[100];	/* Paths, etc.... */
    FILE  *fd;
    int   c, d, index;
    extern char *READ_TEXT;

    strcpy(filename, name);

    if ((fd = fopen(filename, READ_TEXT)) == NULL) {
	return FALSE;
    }
    index = strLen(mbtext);
    while ((c = fgetc(fd)) != EOF && index < MAXTEXT - 2) {
	if (c) {
	    if (c == '\n') {
		/*
		 * this should shave off trailing spaces.
		 */
		while (index - 1 >= 0 && mbtext[index - 1] == ' ')
		    index--;

		while (!(d = fgetc(fd)))   /* skip any following zero bytes */
			;

		if (d == '\n' || d == ' ' || d == EOF) {
		    mbtext[index++] = c;
		    if (d != EOF)
			mbtext[index++] = d;
		}
		else if (d) {
		    mbtext[index++] = ' ';
		    mbtext[index++] = d;
		}
	    }
	    else mbtext[index++] = c;
	}
    }
    mbtext[index] = 0;
    fclose(fd);
    CleanEnd(mbtext);
    return TRUE;
}

/*
 * putBufChar()
 *
 * This is used to upload messages via protocol.
 * returns: ERROR on problems else TRUE.
 */
int putBufChar(int c)
{
    char result;

    if (masterCount == MAXTEXT + 10) return TRUE;

    if (masterCount > MAXTEXT - 2) return ERROR;

	/* This is necessary for a ProComm bug */
    if (c == CPMEOF) {
	masterCount = MAXTEXT + 10;
	return TRUE;
    }

    c &= 0x7F;					/* strip high bit	*/
    result = cfg.filter[c];
    if (result == '\0') {
	return TRUE;
    }
    msgBuf.mbtext[masterCount++] = result;
    msgBuf.mbtext[masterCount]   = 0;   /* EOL just for luck    */
    return TRUE;
}

/*
 * putFLChar()
 *
 * This is used to upload files.
 * returns: ERROR on problems else TRUE.
 */
int putFLChar(int c)
{
    extern FILE *netLog;

    if (fputc(c, upfd) != EOF)  return TRUE;
    /* else */			splitF(netLog,"Write error: %d\n",ferror(upfd));
				return ERROR;
}

static int ShowIgnoredUser(int);
static int IgnoreListUser(char *data, int g);
static int UnignoreListUser(char *data, int g);
/*
 * reconfigure()
 *
 * This function reconfigures a user, depending on their selection on the
 * dot command.
 *
 * Note: returns TRUE on backspace, FALSE otherwise
 */
char reconfigure()
{
    char  *ON  = "ON", *OFF = "OFF";
    label alias, domain;
    char  system[(2 * NAMESIZE) + 10];
    ForwardMail *address;
    int cost;
    extern int thisNet;
    char *ConfgOpts[] = {
	"Complete Reconfigure", "Expert\n", "Floor mode\n",
	"Half-duplex mode\n", "Ignore Mail From User",
	"Linefeeds\n", "Nulls",
	"Old messsage on new\n", "Time of messages\n", "Delay",
	"Width of screen (columns)\n", "\r", "\n", "Z\bOld .RE\n", "\b",
	"Mail Forwarding\n", "Y\bPrompt (message entry)\n",
	"Page Length",
	" ", " ", ""
    };
    static char *NOW = "Now %s.";

    RegisterThisMenu("confg.mnu", ConfgOpts);

    if (cfg.BoolFlags.netParticipant)
	ExtraOption(ConfgOpts, "Address\n");

    if (!cfg.BoolFlags.NoMeet && loggedIn) {
	ExtraOption(ConfgOpts, "Biography");
    }

    switch (GetMenuChar()) {
    case '\b': mPrintf(" \b"); PushBack('\b'); return TRUE;
    case 'A':			/* Forwarding address on the network */
	if (!ReqNodeName("system to forward Mail> to", alias, domain, RNN_ASK,
			&netBuf) && onLine()) {	/* in case carrier is lost */
	    if (SearchList(&MailForward, logBuf.lbname) == NULL)
		break;
	    else if (getYesNo("Stop forwarding Mail>")) {
		KillData(&MailForward, logBuf.lbname);
		UpdateForwarding();
		break;
	    }
	}
	else if (onLine()) {	/* in case carrier is lost */
	    sprintf(system, (strLen(domain) != 0) ? "%s _ %s" : "%s%s",
					alias, domain); 
	    if (strLen(domain) != 0) cost = FindCost(domain);
	    else cost = !netBuf.nbflags.local;
	    if (cost > logBuf.credit)
		mPrintf(CREDIT_WARN, system);
	    sprintf(msgBuf.mbtext, "alias to forward to (C/R=%s)", 
							logBuf.lbname);
	    getString(msgBuf.mbtext, alias, NAMESIZE, 0);
	    if (onLine()) {	/* i.e., didn't drop carrier */
		AddMailForward(logBuf.lbname, system, 
				(strLen(alias) != 0) ? alias : logBuf.lbname);
		if (!logBuf.lbflags.NET_PRIVS)
		    mPrintf("Warning: you do not have net privileges!\n ");
	    }
	}
	break;
    case 'M':
	getString("Forward to which account", alias, NAMESIZE, 0);
	if (strLen(alias) != 0) {
	    if (findPerson(alias, &logTmp) == ERROR)
		mPrintf("No such person\n ");
	    else if (strCmpU(logTmp.lbname, logBuf.lbname) == SAMESTRING)
		mPrintf("That's you!\n ");
	    else
		AddMailForward(logBuf.lbname, NULL, alias);
	}
	else if (getYesNo("Stop forwarding Mail>")) {
	    KillLocalFwd(logBuf.lbname);
	}
	break;
    case 'I':
	getList(IgnoreListUser, "Users to ignore", NAMESIZE, FALSE, 0);
	getList(UnignoreListUser, "Users to NOT ignore", NAMESIZE, FALSE, 0);
#ifdef HERE
	if (strLen(alias) != 0) {
	    if ((cost = findPerson(alias, &logTmp)) == ERROR)
		mPrintf("No such person\n ");
	    else if (!IgnoreThisUser(cost))
		mPrintf("Failure!\n ");
	}
#endif
	break;
    case 'B':
	EditBio();
	break;
    case 'C':
	configure(&logBuf, FALSE, FALSE);
	break;
    case 'D':
	logBuf.lbdelay   = (int) getNumber("millisecond delay", 0l, 255l);
	break;
    case 'E':
	mPrintf(NOW, (expert = !expert) ? ON : OFF);
	break;
    case 'P':
	logBuf.lbpage = (int) getNumber("page length (0 to disable)", 0l, 255l);
	logBuf.lbflags.MSGPAGE = getYesNo("Do Message Paging");
	break;
    case 'Y':
	mPrintf(NOW,(logBuf.lbflags.NoPrompt=!logBuf.lbflags.NoPrompt) ? OFF : ON);
	break;
    case 'Z':
	mPrintf(NOW, (logBuf.lbflags.ALT_RE = !logBuf.lbflags.ALT_RE) ? ON : OFF);
	break;
    case 'F':
	mPrintf(NOW, (FloorMode = !FloorMode) ? ON : OFF);
	break;
    case 'H':
	mPrintf(NOW, (HalfDup = !HalfDup) ? ON : OFF);
	break;
    case 'L':
	mPrintf(NOW, (termLF = !termLF) ? ON : OFF);
	break;
    case 'N':
	termNulls   = (int) getNumber("#Nulls (normally 0)", 0l, 255l);
	break;
    case 'O':
	mPrintf(NOW, (oldToo = !oldToo) ? ON : OFF);
	break;
    case 'T':
	mPrintf(NOW, (sendTime = !sendTime) ? ON : OFF);
	break;
    case 'W':
	termWidth   = (int) getNumber("screen width", 10l, 255l);
	break;
    case '\r':
    case '\n':
	PagingOn();
	printHelp("confg.mnu", HELP_SHORT);
    case '?':
	mPrintf("\n Your current setup:\n ");
	mPrintf("%s, ", (expert) ? "Expert" : "Non-expert");

	mPrintf("%sinefeeds, %d nulls,",
		termLF     ?  "L" : "No l",
		termNulls
		);
	mPrintf(" screen width is %d\n ", termWidth);
	mPrintf("%s time messages created,\n ",
		sendTime ? "Print" : "Do not print");
	mPrintf("%s last Old message on <N>ew Message request.",
		oldToo ? "Print" : "Do not print");
	if (HalfDup) mPrintf("\n Using Half-Duplex mode.");
	if (FloorMode) mPrintf("\n FLOOR mode.");
	if (logBuf.lbdelay) mPrintf("\n Delay of %d milliseconds.",
							logBuf.lbdelay);
	if (logBuf.lbpage) mPrintf("\n Page Length %d.", logBuf.lbpage);
	if (logBuf.lbflags.MSGPAGE) mPrintf("\n Page by message.");
	if (logBuf.lbflags.ALT_RE)
	    mPrintf("\n Using Alternate .RE.");

	if (cfg.BoolFlags.netParticipant &&
		(address = SearchList(&MailForward, logBuf.lbname)) != NULL)
	    mPrintf("\n net Forward Mail> to %s @%s.", address->Alias,
							address->System);
	if (FindLocalForward(logBuf.lbname) != NULL)
	    mPrintf("\n Forward Mail> to local account %s.",
					FindLocalForward(logBuf.lbname));
	IgnoredShown = FALSE;
	IgnoredUsers(thisLog, ShowIgnoredUser);
	if (IgnoredShown) mPrintf("\b\b."); 
	mPrintf("\n ");
	PagingOff();
	break;
    }
    return FALSE;
}

static int IgnoreListUser(char *alias, int g)
{
	int slot;

	if ((slot = findPerson(alias, &logTmp)) == ERROR)
		mPrintf("No such person\n");
	else if (!IgnoreThisUser(slot))
		mPrintf("Failure!\n ");
	return TRUE;
}

static int UnignoreListUser(char *alias, int g)
{
	int slot;

	if ((slot = findPerson(alias, &logTmp)) == ERROR)
		mPrintf("No such person\n");
	else if (!IgMailRemoveEntries(thisLog, slot))
		mPrintf("Failure!\n ");
	return TRUE;
}

/*
 * ShowIgnoredUser
 *
 * This is a callback function used to display a user that is being ignored by
 * the current user.
 */
static int ShowIgnoredUser(int user)
{
	if (!IgnoredShown)
		mPrintf("\n Ignored users: ");
	IgnoredShown = TRUE;
	getLog(&logTmp, user);
	mPrintf("%s, ", logTmp.lbname);
	return TRUE;
}

/*
 * SaveInterrupted()
 *
 * This saves an interrupted message.
 */
void SaveInterrupted(MessageBuffer *SomeMsg)
{
    SYS_FILE temp;
    SYS_FILE save_mess;

    if (cfg.BoolFlags.HoldOnLost) {
	sprintf(temp, LCHeld, thisLog);
	makeSysName(save_mess, temp, &cfg.holdArea);
	if (access(save_mess, 0) == -1) {
	    if ((upfd = fopen(save_mess, WRITE_ANY)) == NULL)
		printf("Failed to open save file!\n");
	    else {
		crypte(SomeMsg, STATIC_MSG_SIZE, thisLog);
		fwrite(SomeMsg, STATIC_MSG_SIZE, 1, upfd);
		crypte(SomeMsg->mbtext, MAXTEXT, thisLog);
		fwrite(SomeMsg->mbtext, MAXTEXT, 1, upfd);
		RunListA(&SomeMsg->mbCC, DisplayCC, (void *) TEXTFILE);
		fclose(upfd);
	    }
	}
    }
}

/*
 * TranFiles()
 *
 * This handles transfer of files to users:
 *
 * 1. Gets number of files, number of bytes.
 * 2. Performs time calculations.
 * 3. Starts up protocols.
 */
void TranFiles(int protocol, char *phrase)
{
    extern unsigned long netBytes;
    int NumFiles;
    char FileSpec[100];

    getNormStr("filename(s)", FileSpec, sizeof FileSpec, 0);
    if (strLen(FileSpec) == 0) return;
    netBytes = 0l;
    NumFiles = wildCard(getSize, FileSpec, phrase,
			WC_DEFAULT | WC_MOVE | WC_NO_COMMENTS);

    if (NumFiles <= 0l) {
	mPrintf("Sorry, no match for '%s'.\n ", FileSpec);
	return;
    }

    if (!TranAdmin(protocol, NumFiles)) return;

    TranSend(protocol, transmitFile, FileSpec, phrase, TRUE);
    unlink(UploadLog);
}

/*
 * TranAdmin()
 *
 * Transfer file administrator.
 */
char TranAdmin(int protocol, int NumFiles)
{
    extern SListBase ExtProtocols;
    PROTOCOL *Prot;
    long seconds;
    extern unsigned long netBytes;
    long s;

    if (NumFiles != 1 && ((InternalProtocol(protocol) &&
			!(Table[protocol].flags & IS_NUMEROUS)) ||
	(!InternalProtocol(protocol) && !DoesNumerous(protocol)))) {
	mPrintf("%s does not support batch mode.\n ",
			(InternalProtocol(protocol)) ?
			Table[protocol].name : FindProtoName(protocol));
	return FALSE;
    }

    if (!InternalProtocol(protocol) || Table[protocol].flags & RIGAMAROLE) {
	mPrintf("This %s transfer involves %s bytes (",
		(!InternalProtocol(protocol)) ? FindProtoName(protocol) :
				Table[protocol].name,
				PrintPretty(netBytes, msgBuf.mbtext));

	mPrintf("%d file%s", NumFiles, NumFiles == 1 ? "" : "s");

	if (InternalProtocol(protocol))
	    mPrintf(" : %ld blocks",
	  ((netBytes+(Table[protocol].BlockSize-1))/Table[protocol].BlockSize));

	mPrintf(").  ");

	if (InternalProtocol(protocol))
	    s = (long) Table[protocol].KludgeFactor;
	else {
	    if ((Prot = SearchList(&ExtProtocols, &protocol)) == NULL)
		s = (long) Table[1].KludgeFactor;
	    else if ((s = Prot->KludgeFactor) == 0)
		s = (long) Table[1].KludgeFactor;
	}

	if (s != 0 && BaudRate != 0l) {
	    seconds = (long) s * netBytes / BaudRate;
	    if (!CheckDLimit(seconds)) return FALSE;
	    mPrintf("It should take %ld:%02ld.\n ", seconds/60, seconds % 60);
	}

	return getYesNo("ready");
    }
    return TRUE;
}

/*
 * TranSend()
 *
 * This does the send work of TranFiles().
 */
void TranSend(int protocol, void (*fn)(DirEntry *f), char *FileSpec,
						char *phrase, char NeedToMove)
{
	DirEntry temp = { "", "", 0l };

	startTimer(WORK_TIMER);
	PagingOn();

	if (InternalProtocol(protocol)) {
		TransProtocol = protocol;

		wildCard((FormatFlag) ? doFormatted : fn, FileSpec, phrase,
			WC_NO_COMMENTS | WC_DEFAULT | (NeedToMove?WC_MOVE:0));

		if (Table[protocol].flags & NEEDS_FIN)
			(*fn)(&temp);
	}
	else {
		ExternalProtocol(protocol, FALSE, FileSpec, phrase, NeedToMove);
	}

	if (!InternalProtocol(protocol) || (Table[protocol].flags & IS_DL))
		*DL_Total += chkTimeSince(WORK_TIMER);

	TransProtocol = ASCII;
	if (!InternalProtocol(protocol)||(Table[protocol].flags & RIGAMAROLE)) {
		oChar(BELL);
	}
	PagingOff();
}

/*
 * transmitFile()
 *
 * This dumps a host file with no formatting.
 */
void transmitFile(DirEntry *file)
{
    FILE *fbuf;
    long fileSize = 0l;
    char *filename, success;
    extern char *READ_ANY;

    filename = file->unambig;
    if (strLen(filename) != 0) {
	if ((fbuf = safeopen(filename, READ_ANY)) == NULL) {
	    return ;
	}
	totalBytes(&fileSize, fbuf);
	if (Table[TransProtocol].flags & RIGAMAROLE)
	    printf("%s: %s (%ld bytes, %ld blocks)\n",
		Table[TransProtocol].name, filename, fileSize,
((fileSize+(Table[TransProtocol].BlockSize-1))/Table[TransProtocol].BlockSize));
	fileMessage(FL_START, filename, TRUE, TransProtocol, 0l);
    }

    if (Transmission(TransProtocol, STARTUP) != TRAN_SUCCESS) {
	fclose(fbuf);
	if (strLen(filename) != 0)
	    fileMessage(FL_FAIL, filename, TRUE, TransProtocol, fileSize);
	return ;
    }

    if (Table[TransProtocol].flags & NEEDS_HDR) {
	if (!(*Table[TransProtocol].SendHdr)(fileSize, filename)) {
	    fclose(fbuf);
	    if (strLen(filename) != 0)
		fileMessage(FL_FAIL, filename, TRUE, TransProtocol, fileSize);
	    return ;
	}
    }

    if (strLen(filename) != 0) {
	SendThatDamnFile(fbuf, Table[TransProtocol].method);
	success = (Transmission(TransProtocol, FINISH) != TRAN_SUCCESS) ?
				FL_FAIL : FL_SUCCESS;
	fileMessage(success, filename, TRUE, TransProtocol, fileSize);
	if (TransProtocol == ASCII && outFlag != OUTOK) {
	    doCR();
	    outFlag = OUTOK;
	}
    }
}

/*
 * SendThatDamnFile()
 *
 * This will actually send the file.
 */
void SendThatDamnFile(FILE *fbuf, int (*method)(int c))
{
    int c;
#ifdef AMIGA
    int oldc = 0;
#endif

    while ((c = fgetc(fbuf)) != EOF && (c != CPMEOF || !textDownload))  {
	if (TransProtocol == ASCII) mputChar(c);
	if (gotCarrier())
	    if (!(*method)(c))
		break;
	if (TransProtocol == ASCII) {
	    if (!onConsole)
		MilliSecPause(logBuf.lbdelay);
	    if (mAbort() || (whichIO == MODEM && !gotCarrier())) break;
	    if (c == '\n') {
		CurLine++;
		if (!MoreWork(FALSE)) break;
	    }
#ifdef AMIGA
	/* make handling text files more sane for non-Amiga people */
	    if (gotCarrier() && c == '\n' && oldc != '\r')
		(*method)('\r');
#endif
	}
#ifdef AMIGA
	oldc = c;
#endif
    }
    fclose(fbuf);
    textDownload = FALSE;
}

void FreeFileEntry(), *EatDSZFormat(char *line);
SListBase FileList = { NULL, NULL, NULL, FreeFileEntry, EatDSZFormat };
/*
 * upLoad()
 *
 * This enters a file into current directory.
 */
void upLoad(int WC)
{
    void BuildReport();
    char fileName[MAX_FILENAME - 1];
    char successful;
    long size;

    if (!DoesNumerous(WC)) {
	getNormStr(strFile, fileName, sizeof fileName, 0);
	if (strLen(fileName) == 0) return;

	    /* Can't tolerate bad file names */
	if (!ValidDirFileName(fileName)) {
	    mPrintf("Illegal file name.\n ");
	    return ;
	}
    }
    else {
	doCR();
	strcpy(fileName, "");
    }

    if (!SetSpace(FindDirName(thisRoom))) {	/* System error -- yucky. */
	return ;
    }

    if (cfg.LowFree != 0 && RoomLeft(thisRoom) < cfg.LowFree) {
	mPrintf("Sorry, not enough room left on disk.\n ");
	homeSpace();
	return ;
    }

    if (!DoesNumerous(WC)) {
	if (access(fileName, 0) != -1) {
					/* File already exists */
	    mPrintf("\n %s already exists.\n", fileName);
	    homeSpace();
	    return;
	}
    }
				/* Go for it */
    if (!expert && InternalProtocol(WC)) {
	homeSpace();
	printHelp(Table[WC].UpBlbName, HELP_SHORT);
	SetSpace(FindDirName(thisRoom));
    }

    if (!getYesNo("Ready for transfer")) {
	homeSpace();
	return;
    }

    if (!InternalProtocol(WC)) {
	fileMessage(FL_START, fileName, FALSE, WC, 0l);
	successful = (ExternalProtocol(WC, TRUE, fileName, NULL, FALSE)
						== TRAN_SUCCESS);
    }
    else {
	/* currently no internal protocols support batch */
	if ((upfd = safeopen(fileName, WRITE_ANY)) == NULL) {
	    mPrintf("\n Can't create %s!\n", fileName);
	    homeSpace();
	    return;
	}
#ifdef HORRID_AMIGA_LATTICE_BUG
	setnbf(upfd);
#endif
	fileMessage(FL_START, fileName, FALSE, WC, 0l);
	successful = (Reception(WC, putFLChar) == TRAN_SUCCESS);
	fclose(upfd);
    }

	/* this signals end of transfer -- for accurate timing purposes */
    fileMessage(FL_FIN, fileName, FALSE, WC, size);

    if (!successful) {
	if (!DoesNumerous(WC)) unlink(fileName);
    }
    else {
	if (!DoesNumerous(WC)) {
	    if (strcmp(fileName, "*"))
	        size = FileCommentUpdate(fileName, TRUE, TRUE);
	}
	else {
	    MakeList(&FileList, UploadLog, NULL);
	    strcpy(msgBuf.mbtext, "   ");
	    RunList(&FileList, BuildReport);
	    KillList(&FileList);
	    unlink(UploadLog);
	}
    }

    homeSpace();

    fileMessage(successful ? DoesNumerous(WC) ? FL_EX_END:FL_SUCCESS: FL_FAIL,
						fileName, FALSE, WC, size);
}

/*
 * BuildReport()
 *
 * This function helps build a report of uploaded files.
 */
static void BuildReport(UploadFile *entry)
{
	sprintf(lbyte(msgBuf.mbtext), "%s (%ld) ", entry->name, entry->size);
}

/*
 * FreeFileEntry()
 *
 * This function frees an entry in the list of uploaded files.
 */
static void FreeFileEntry(UploadFile *entry)
{
	free(entry->name);
	free(entry);
}

/*
 * EatDSZFormat()
 *
 * Eat a line from a DSZ log file
 */
static void *EatDSZFormat(char *line)
{
	extern int offline_mode;
	char *arg, *tok;
	int rover;
	long size;
	UploadFile *entry;

	if ((tok = strtok(line, " \t")) == NULL) return NULL;

	if (!isupper(*tok) || *tok == 'E' || *tok == 'L')
		return NULL;

	for (rover = 1; rover < 11; rover++) {
		tok = strtok(NULL, " \t");
		if (tok == NULL) return NULL;
	}

	if ((arg = strrchr(tok, '\\')) == NULL) 
		arg = tok;
	else
		arg++;

	size = FileCommentUpdate(arg, TRUE, !offline_mode);
	entry = GetDynamic(sizeof *entry);
	entry->name = strdup(arg);
	entry->size = size;
	/* sprintf(msgBuf.mbtext, "%s (%ld) ", arg, size); */
	return entry;
}

/*
 * FileCommentUpdate()
 *
 * This updates the file comment.
 */
long FileCommentUpdate(char *fileName, char aideMsg, char getcomment)
{
	char *tmp = NULL, buffer[20];
	long size = 0;
	FILE *fd;

	if (getcomment && !FileIntegrity(fileName)) {
		unlink(fileName);
		return 0;
	}

	if ((fd = fopen(fileName, READ_ANY)) != NULL) {
		totalBytes(&size, fd);
		fclose(fd);
	}

	if (!getcomment) return size;

	do {
		msgBuf.mbtext[0] = 0;
		mPrintf("Please enter a description of %s (end with blank line).\n ",
				fileName);
		doCR();
	} while (onLine() &&
		!GetBalance(ASCII, msgBuf.mbtext, MAXTEXT-50, FILE_ENTRY,
				fileName));

	while ((tmp = strchr(msgBuf.mbtext, '\r')) != NULL)
		*tmp = ' ';
	while ((tmp = strchr(msgBuf.mbtext, '\n')) != NULL)
		*tmp = ' ';
	if (aideMsg || strLen(msgBuf.mbtext) != 0) {
		if (loggedIn && !roomBuf.rbflags.ANON)
			sprintf(lbyte(msgBuf.mbtext), " [%s].", logBuf.lbname);
		updFiletag(fileName, msgBuf.mbtext);
	}
	if (aideMsg || strLen(msgBuf.mbtext) != 0) {
		homeSpace();
		tmp = strdup(msgBuf.mbtext);
		ZeroMsgBuffer(&msgBuf);
		sprintf(msgBuf.mbtext, "File \"%s\" (%s bytes) uploaded into %s.",
				fileName, PrintPretty(size, buffer),
				formRoom(thisRoom, FALSE, FALSE));
		if (loggedIn)
			sprintf(lbyte(msgBuf.mbtext) - 1, " by %s.",
							logBuf.lbname);
		if (aideMsg) aideMessage(NULL,FALSE);
		strcpy(msgBuf.mbauth, "Citadel");
		sprintf(msgBuf.mbtext,"File \"%s\" (%s bytes) uploaded into %s:\n \n%s",
				fileName, PrintPretty(size, buffer),
				formRoom(thisRoom, FALSE, FALSE), tmp);
		putMessage(&logBuf, 0);	/* Now save message in this room*/
		noteRoom();
		SetSpace(FindDirName(thisRoom));
	}
	if (tmp != NULL) free(tmp);
	return size;
}

/*
 * visible()
 *
 * This converts given char to printable form if nonprinting.
 */
char visible(AN_UNSIGNED c)
{
    if (c==0xFF)  c = '$'	;   /* start-of-message in message.buf  */
    c		    = c & 0x7F  ;   /* kill high bit otherwise		*/
    if ( c < ' ') c = c + 'A' -1;   /* make all control chars letters   */
    if (c== 0x7F) c = '~'	;   /* catch DELETE too			*/
    return(c);
}


char *Menu = NULL;
char **ValidMenuOpts;

/*
 * GetMenuChar()
 *
 * This will get a character for a menu.
 */
int GetMenuChar()
{
    int c, i;

    c = toUpper(iChar());

    for (i = 0; ValidMenuOpts[i][0]; i++)
	if (c == ValidMenuOpts[i][0])
	    break;

    if (!ValidMenuOpts[i][0]) {
	if (!onLine() || (expert && c != '?')) {
	    c = 0;
	    mPrintf(" ?\n ");
	}
	else {
	    c = '?';
	    if (Menu != NULL) printHelp(Menu, HELP_SHORT);
	}
    }
    else mPrintf("%s ", ValidMenuOpts[i] + 1);

    return c;
}

/*
 * ExtraOption()
 *
 * This adds an option to a menu.
 */
void ExtraOption(char *Opts[], char *NewOpt)
{
    int i;

    for (i = 0; Opts[i][0] && Opts[i][0] != ' '; i++)
	;

    if (!Opts[i][0]) {
	crashout("INTERNAL: No room for new option!");
    }

    Opts[i] = NewOpt;
}

static int  MCount;
/*
 * CmdMenuList()
 *
 * This reads in a command line, doing backspacing.
 */
int CmdMenuList(char *Opts[], SListBase *Selects, char *HelpFile, char *buf,
						char moreYet, char OneMore)
{
    int c, rover;
    char *cmd;
    void CopyBuf();

    do {
	c = toUpper(iChar());
	if (c == '\b') {
	    mPrintf(" \b");
	    if ((cmd = GetLast(Selects)) == NULL) {
		if (!OneMore) oChar(' ');
		return BACKED_OUT;
	    }
	    for (rover = 0; rover < strlen(cmd) - 1; rover++)
		if (cmd[rover] != '\b') mPrintf("\b \b");
		else oChar(' ');
	    KillData(Selects, cmd);
	    if (OneMore)
		mPrintf("\b \b");
	}
	else {
	    for (rover = 0; Opts[rover][0] != 0; rover++)
		if (toUpper(Opts[rover][1]) == toupper(c))
		    break;

	    if (c == 0 || Opts[rover][0] == 0 || 
				 SearchList(Selects, Opts[rover] + 1) != NULL) {
		if (!onLine() || (expert && c != '?')) {
		    mPrintf(" ?\n ");
		}
		else if (HelpFile != NULL) {
		    printHelp(HelpFile, HELP_SHORT);
		}
		else mPrintf(" ? (Type '?' for menu)\n \n"   );
		buf[0] = 1;		/* general error */
		return BAD_SELECT;
	    }
	    mPrintf("%s ", Opts[rover] + 2);
	    AddData(Selects, Opts[rover] + 1, NULL, FALSE);
	}
    } while ((c == '\b' || Opts[rover][0] != TERM[0]) && moreYet);
    MCount = 0;
    RunListA(Selects, CopyBuf, (void *) buf);
    return GOOD_SELECT;
}

void CopyBuf(char *name, char *buf)
{
    buf[MCount++] = name[0];
    buf[MCount] = 0;
}

/*
 * FindSelect()
 *
 * This function will find an element of a command selection.
 */
void *FindSelect(char *element, char *data)
{
    if (element == data) return element;
    return NULL;
}

/*
 * PagingOn()
 *
 * Activates paging.
 */
void PagingOn()
{
	if (!Pageable) CurLine = 1;
	Pageable = TRUE;
	Continuous = FALSE;
}

/*
 * ReadMessageSpec(char *source, OptValues *opt)
 *
 * This function is responsible for evaluating the string (source) the user
 * typed in for a .Read <NFOR> <message spec>.  Two message specs are
 * currently supported:
 *
 * - Date spec.  Indicates messages before or after (as determined by the
 *   <nfor> command) the specified date should be shown.
 *
 * - Message Count ("#<number>").  Indicates only the specified number of
 *   messages should be shown.
 */
int ReadMessageSpec(char *source, OptValues *opt)
{
	if (ReadDate(source, &opt->Date) != ERROR)
		return TRUE;

	if (source[0] == '#')
	{
		opt->MaxMessagesToShow = atoi(source + 1);
		if (opt->MaxMessagesToShow > 0)
		{
			return TRUE;
		}
	}

	return ERROR;
}

