/*
 *				sysdep1.c
 *
 * This is the repository of most of the system dependent MS-DOS code in
 * Citadel. We hope, pray, and proselytize, at least.
 */

/*
 *				history
 *
 * 87Apr01 HAW  File tagging completed; bug for .RD/.RE fixed.
 * 86Dec14 HAW  Reorganized into areas.
 * 86Nov25 HAW  Created.
 */

#define SYSTEM_DEPENDENT
#define TIMER_FUNCTIONS_NEEDED
#define NET_INTERFACE
#define NET_INTERNALS

#include "ctdl.h"
#include "sys\stat.h"

/*
 *				Contents
 *
 *		AREAS:
 *	bigDirectory()		gets an "extended" directory
 * #	bigDirs()		work function for bigDirectory()
 *	CitGetFileList()	gets list of files
 *	freeFileList()		Free file list
 *	ValidArea()		validate an area choice
 *	goodArea()		get good area
 *	homeSpace()		takes us to our home space
 *	netGetArea()		get area for dumping networked file(s)
 *	prtNetArea()		makes human readable form of NET_AREA
 *	SetSpace()		goto specified "area"
 * #	realSetSpace()		does work of SetSpace, others
 * #	MSDOSparse()		parses a string for MSDOS filename
 * #	fileType()		gets file type for MSDOS
 *	sysGetSendFiles()       specify where to send files from
 *	updFiletag()		updates a filetag
 *	sysRoomLeft()		how much room left in net recept area
 * #	getSize()		gets size of a file
 *	sysSendFiles()		system dep stuff for sending files
 * #	doSendWork()		does work of sysSendFiles()
 *		BAUD HANDLER:
 * #	check_CR()		scan input for carriage returns
 *	Find_baud()		does flip flop search for baud
 *	getNetBaud()		check for baud of network caller
 *		CONSOLE HANDLING:
 *	KBReady()		returns TRUE if a console char is ready
 *	mputChar()		Do our own for some Console output
 *
 *		# == local for this implementation only
 */
#define SETDISK		14      /* MSDOS change default disk function   */

#define NO_IDEA		0
#define SINGLE_FILE	1
#define IS_DIR		2
#define AMB_FILE	3

/*
 * Globals -- there shouldn't be anything here but statics and externs.
 */
/* These MUST be defined! */
char *R_W_ANY     = "r+b";
char *READ_ANY    = "rb";
char *READ_TEXT   = "rt";
char *APPEND_TEXT = "a+";
char *APPEND_ANY  = "a+b";
char *A_C_TEXT    = "a+";
char *WRITE_TEXT  = "wt";
char *W_R_ANY     = "w+b";
char *WRITE_ANY   = "wb";

char *SysVers = "";

/* Now we need to fool with the stack size, Turbo C style */
extern	unsigned	_Cdecl _stklen = 8000;

/* Here's the rest of the goo */
extern logBuffer logBuf;	/* Log buffer of a person       */
extern SListBase ResList;
extern aRoom     roomBuf;
extern MessageBuffer      msgBuf;
extern CONFIG    cfg;	/* Lots an lots of variables    */
extern int  thisRoom;
extern char onConsole;	/* Who's in control?!?	*/
extern char whichIO;	/* CONSOLE or MODEM	*/
extern char anyEcho;
extern char echo;
extern FILE *upfd;
extern char modStat;
extern char echoChar;
extern char haveCarrier;
extern long netBytes;
extern char outFlag;
extern char *strFile;
extern char *indexTable;

/*
 * Section 3.4. AREAS:
 *   The model of Citadel includes a provision for "directory rooms."
 * A directory room is defined as the ability to look in on some section
 * of the host system's file section.  In order to avoid tying the
 * directory structure of Citadel to any particular operating system, an
 * "abstraction" has been implemented.  Each room that has been desig-
 * nated as a directory will have an "area" associated with it.  This
 * "area" is dependent on each implementation; access to "areas" is
 * through routines located in this module.  Therefore, the abstract
 * directory model of Citadel is in the main code modules, and should
 * not require any changes from port to port.  The only changes neces-
 * sary should be in this file (SYSDEP.C), where the porter must decide
 * upon and implement a mapping of how a directory room peeks in on his
 * or her file system.
 *   Basically, the routines are fairly simple in purpose.
 *
 *      "And if pigs had wings they could fly!"
 */

/*
 * MSDOS version:
 *    MSDOS has a "tree" directory structure, one tree for each disk
 * on the system.  The system remembers for each tree that there is a
 * "current directory" associated with it.  For this implementation,
 * directory rooms may either peek in on the current directory of any
 * disk on the system, or any subdirectory of the current directory of
 * any disk.  Directories above the current directory of any disk cannot
 * be accessed via directory rooms.  However, network area handlers do
 * allow access.
 */

char    locDir[100] = "";
char    ourHomeSpace[100];
char    locDisk;

static char *flDir = "filedir.txt";
static FILE *fileTags;		/* For the file tags	*/
static char tagFound = FALSE;

/*
 * ValidDirFileName()
 *
 * This should validate a file name.
 */
char ValidDirFileName(char *fn)
{
    char *work, *c, toReturn = TRUE;

    if (strchr(fn, ':')  != NULL ||
	strchr(fn, '\\') != NULL ||
	strchr(fn, '/')  != NULL ||
	strchr(fn, ' ')  != NULL)
	return FALSE;

    work = strdup(fn);
    c = strchr(work, '.');
    if (c != NULL) *c = 0;
    if (strlen(work) > 8) toReturn = FALSE;
    if (c != NULL) {
	if (strlen(c + 1) > 3) toReturn = FALSE;
	if (strchr(c + 1, '.') != NULL) toReturn = FALSE;
    }

    return toReturn;
}

/*
 * RoomLeft()
 *
 * Just how much room left in this area, anyways?
 * NB: Returns in K, not bytes!
 */
long RoomLeft(int room)
{
    char *temp, dir[150], drive;
    long temp3, temp2;
    long toReturn;

    /* this should never happen, but just in case ... */
    if ((temp = FindDirName(room)) == NULL)
	return 0;

    strCpy(dir, temp);
    MSDOSparse(dir, &drive);
    diskSpaceLeft(drive, &temp3, &temp2);
    toReturn = temp2 / 1024l;
    return toReturn;
}

/*
 * CitGetFileList()
 *
 * This should get a list of files from the current "area".
 * Returns the # of files listed that fit the given mask.
 */
static char *Phrase;

void CitGetFileList(char *mask, SListBase *list, long before, long after,
								char *phrase)
{
    struct ffblk   FlBlock;
    extern char    *monthTab[13];
    char	   *w, *work, *sp, again, buf[10];
    DirEntry       *fp;
    int		   done;
    struct stat    buff;
    long	   time;

    w = work = strdup(mask);

    do {
	again = (sp = strchr(work, ' ')) != NULL;      /* space is separator */
	if (again) {
	    *sp = 0;
	}
	/* Do all scanning for illegal requests here */
	if (!ValidDirFileName(work))
	    continue;

	/* this checks for illegal file names like CON:, PRN:, etc. */
	if (stat(work, &buff) == 0)
	    if (buff.st_mode & S_IFCHR)
		continue;

	for (done = findfirst(work, &FlBlock, 0); !done; 
						done = findnext(&FlBlock)) {
	    /* format date to our standards */
	    DosToNormal(buf, FlBlock.ff_fdate);

	    /* now read it so we can handle date specs */
	    ReadDate(buf, &time);

	    /* add to list iff dates inactive or we meet the specs */
	    if ((before != -1l && time > before) ||
				(after  != -1l && time < after))
		continue;

	    fp = (DirEntry *) GetDynamic(sizeof *fp);

	    fp->unambig = strdup(FlBlock.ff_name);

	    strCpy(fp->FileDate, buf);
	    fp->FileSize = FlBlock.ff_fsize;

	    AddData(list, fp, NULL, TRUE);
	}
	if (again) work = sp+1;
    } while (again);
    free(w);

    if (strlen(phrase) != 0) {
	Phrase = phrase;
	StFileComSearch();      /* so's we can do phrase searches */
	list->CheckIt = ChPhrase;   /* COVER YOUR EYES I'M CHEATING! */
	KillData(list, list);
	list->CheckIt = DirCheck;   /* COVER YOUR EYES I'M CHEATING! */
	EndFileComment();
    }
}

#include "compress.h"
/*
 * ChPhrase()
 *
 * This is necessary to search the files found for a phrase either in their
 * file comments or in the data part of a 'strange' file (GIF, etc).
 */
void *ChPhrase(DirEntry *e, DirEntry *d)
{
    FILE *fd;
    int  format;
    char buf[100];
    extern FunnyInfo Formats[];

    if (!FindFileComment(e->unambig, FALSE) || 
	matchString(msgBuf.mbtext, Phrase, lbyte(msgBuf.mbtext)) == NULL) {
	/*
	 * This code extracts information from the GIF, etc file and
	 * matches it against the given phrase.
	 */
	    format = CompressType(e->unambig);
	    if (format == ERROR || Formats[format].Many)
		return d;

	    if ((fd = fopen(e->unambig, READ_ANY)) == NULL)
		return d;

	    (*Formats[format].Func)(fd, FALSE, buf);
	    fclose(fd);
	    if (matchString(buf, Phrase, lbyte(buf)) == NULL)
		return d;
    }
    return NULL;
}

/*
 * ValidArea()
 *
 * This will validate an area choice the system operator has made.
 */
char ValidArea(char *area)
{
    char drive, *work, c;

    MSDOSparse(area, &drive);
    c = fileType(area, drive);

    if (!CheckArea(NO_MENU, c, &drive, area))
	return FALSE;

    work = GetDynamic(strlen(area) + 5);
    sprintf(work, "%c:%s", drive, area);
    strcpy(area, work);
    free(work);
    return TRUE;
}

/*
 * goodArea()
 *
 * This will get a valid path from the sysop. Drive should be set already.
 */
static goodArea(MenuId id, char *prompt, char *dir, char *drive)
{
    int  c;
    char dir_x[150], *dft;

    while (TRUE) {
	if (roomBuf.rbflags.ISDIR)
	    dft = FindDirName(thisRoom);
	else
	    dft = ourHomeSpace;

	if (!getXInternal(id, prompt, dir_x, 149, dft, dft))
	    return FALSE;

	MSDOSparse(dir_x, drive);
	c = fileType(dir_x, *drive);

	if (CheckArea(id, c, drive, dir_x)) {
	    strCpy(dir, dir_x);
	    return TRUE;
	}
    }
}

/*
 * CheckArea()
 *
 * This function deals with the question of whether the given string is a
 * directory and to create it if possible and needed.
 */
char CheckArea(MenuId id, char c, char *drive, char *dir_x)
{
    char work[100];

    switch (c) {
    case IS_DIR:
	return TRUE;
    case NO_IDEA:
	if (strlen(dir_x) != 0) {
	    sprintf(work, "%s does not exist. Create it", dir_x);
	    if (SysopGetYesNo(id, "", work)) {
		DoBdos(SETDISK, toUpper(*drive) - 'A');
		if (mkdir(dir_x) == BAD_DIR) {
		    SysopPrintf(id, "?ERROR CREATING!");
		    homeSpace();
		    return FALSE;
		}
		else {
		    homeSpace();
		    return TRUE;
		}
	    }
	}
	else {
	    return TRUE;
	}
	return FALSE;
    default:
	SysopPrintf(id, "That's not a directory!\n ");
	return FALSE;
    }
}

/*
 * homeSpace()
 *
 * takes us home!
 */
void homeSpace()
{
    if (strlen(locDir) != 0) {
	chdir(locDir);
	locDir[0] = 0;
    }
    DoBdos(SETDISK, locDisk - 'A');
    chdir(ourHomeSpace);
}

/*
 * netGetAreaV2()
 *
 * This gets an area for storing a file.  It handles sysop menus.
 */
int netGetAreaV2(MenuId id, char *fn, struct fl_req *file_data, char ambiguous)
{
    char goodname;

    if (!goodArea(id, "\ndirectory for storage",
			file_data->flArea.naDirname,
			&file_data->flArea.naDisk)) return FALSE;

    file_data->flArea.naDisk -= 'A';
    if (!ambiguous) {
	realSetSpace(file_data->flArea.naDisk, file_data->flArea.naDirname);
	do {
	    if (!getXInternal(id,
			"\nname the file will be stored under",
			file_data->filename, NAMESIZE, fn, fn)) {
		homeSpace();
		return FALSE;
	    }
	    if (access(file_data->filename, 0) == 0) {
		SysopPrintf(id, "'%s' already exists, and ",
					file_data->filename);
		SysopPrintf(id, "will be overwritten during networking.");
		goodname = SysopGetYesNo(id, "", "Is this what you want");
	    }
	    else goodname = TRUE;
	} while (!goodname);
	homeSpace();
    }
    else {
	SysopPrintf(id, "Ambiguous requests may result in overwriting.\n ");
	file_data->filename[0] = 0;
    }
    return TRUE;
}

/*
 * prtNetArea()
 *
 * This produces a human-readable form of a NET_AREA.
 */
char *prtNetArea(NET_AREA *netArea)
{
    static char temp[105];

    sprintf(temp, "%c:%s", netArea->naDisk + 'A', netArea->naDirname);
    return temp;
}

/*
 * realSetSpace()
 *
 * This does the real work of SetSpace.
 */
char realSetSpace(char disk, char *dir)
{
    DoBdos(SETDISK, disk);

    getcwd(locDir, 99);

    if (strlen(dir) != 0 && chdir(dir) == BAD_DIR) {
	homeSpace();
	return FALSE;
    }

    return TRUE;
}

/*
 * SetSpace()
 *
 * This moves us to an area associated with the specified room.
 */
char SetSpace(char *area)
{
    char   dir[150], drive;

    if (area == NULL) {
	mPrintf("?Directory not present! (internal error)\n ");
	return FALSE;
    }
    strCpy(dir, area);
    MSDOSparse(dir, &drive);
    if (!realSetSpace(toUpper(drive) - 'A', dir)) {
	mPrintf("?Directory not present!\n ");
	return FALSE;
    }
    return TRUE;
}

/*
 * MSDOSparse()
 *
 * This parses a string designating a file.
 */
void MSDOSparse(char *theDir, char *drive)
{
    if (theDir[1] == ':') {
	*drive = toUpper(theDir[0]);
	strCpy(theDir, theDir+2);
    }
    else {
	*drive = locDisk;
    }
}

/*
 * fileType()
 *
 * This gets the file type for a MSDOS file.
 */
static fileType(char *theDir, char theDrive)
{
    char name[100];
    struct stat buf;

    sprintf(name, "%c:%s", theDrive, theDir);
    if (strchr(theDir, '*') != NULL ||
	strchr(theDir, '?') != NULL) {
	return AMB_FILE;
    }
    if (stat(name, &buf) == 0) {
	if (buf.st_mode & S_IFDIR) return IS_DIR;
	if (buf.st_mode & S_IFREG) return SINGLE_FILE;
    }
    /* ugly cheat due to DOS bug -- stat("C:\", ...) doesn't work */
    if (strlen(name) == 3 && name[2] == '\\') return IS_DIR;
    return NO_IDEA;
}

/*
 * sysGetSendFilesV2()
 *
 * This function figures out where to find files to send to another system.
 */
char sysGetSendFilesV2(MenuId id, char *name, struct fl_send *sendWhat)
{
    strCpy(sendWhat->snArea.naDirname, name);

    MSDOSparse(sendWhat->snArea.naDirname, &sendWhat->snArea.naDisk);
    if (fileType(sendWhat->snArea.naDirname,
		sendWhat->snArea.naDisk) == NO_IDEA) {
	if (SysopGetYesNo(id, "",
			"Neither file nor directory found. Send anyways"))
	    return TRUE;
	return FALSE;
    }
    return TRUE;
}

/*
 * CopyFile()
 *
 * This function copies a file into a room.
 */
char CopyFile(char *oldname, int room, long *size)
{
    struct stat buf;
    char *temp;
    char buffer[150];

    if ((temp = FindDirName(room)) == NULL) {
	mPrintf("Can't find room's directory.\n ");
	return FALSE;
    }

    if (stat(oldname, &buf) != 0 || !(buf.st_mode & S_IFREG)) {
	mPrintf("Is not a copyable file.\n ");
	return FALSE;
    }

    *size = buf.st_size;

    CitSystem(FALSE, "copy %s %s > nul", oldname, temp);

    if ((temp = strrchr(oldname, '\\')) != NULL)
	strcpy(oldname, temp + 1);
    else if (oldname[1] == ':')
	strcpy(oldname, oldname + 2);

    temp = FindDirName(room);

    sprintf(buffer, "%s%s%s", temp, (temp[strlen(temp) - 1] == ':') ? "" : "\\",
								oldname);

    return (access(buffer, 0) == 0);
}

/*
 * CopyFileGetComment()
 *
 * This function tries to get the file comment related to the specified full
 * pathname.
 */
int CopyFileGetComment(char *fname, int thisRoom, char *comment)
{
	long size;
	char *dup, *temp;
	int result;

	dup = strdup(fname);
	if (!CopyFile(fname, thisRoom, &size)) {
		free(dup);
		return FALSE;
	}

	if ((temp = strrchr(dup, '\\')) != NULL) {
		*temp = 0;
		sprintf(msgBuf.mbtext, "%s\\%s", dup, flDir);
	}
	else if (dup[1] == ':') {
		sprintf(msgBuf.mbtext, "%s%s", dup, flDir);
	}
	else {
		strcpy(msgBuf.mbtext, flDir);
	}

	tagFound = TRUE;
	if ((fileTags = fopen(msgBuf.mbtext, READ_TEXT)) != NULL) {
		msgBuf.mbtext[0] = 0;
		result = FindFileComment(fname, FALSE);
		EndFileComment();	/* closes fileTags, too */
		if (!result) comment[0] = 0;
		else strcpy(comment, msgBuf.mbtext);
	}
	else comment[0] = 0;

	free(dup);
	return TRUE;
}

/*
 * MoveFile()
 *
 * This function will move a file to a new name and location.
 */
void MoveFile(char *oldname, char *newname)
{
    char buffer[100];
    FILE *infd, *outfd;
    int s;

    if (toUpper(*oldname) == toUpper(*newname)) {
	rename(oldname, newname);
	if (access(newname, 0) == 0) return;
    }
    CitSystem(FALSE, "copy %s %s > nul", oldname, newname);
    if (access(newname, 0) == 0) {
	unlink(oldname);
    }
    else {
	if ((infd = fopen(oldname, READ_ANY)) == NULL) return;
	if ((outfd = fopen(newname, WRITE_ANY)) == NULL) {
	    fclose(infd);
	    return;
	}
	while ((s = fread(buffer, sizeof buffer, 1, infd)) > 0)
	    fwrite(buffer, s, 1, outfd);
	fclose(infd);
	fclose(outfd);
	unlink(oldname);
    }
}

/*
 * sysRoomLeft()
 *
 * Just how much room is left in the net recept area.
 */
long sysRoomLeft()
{
    long temp, temp2, temp3;

    temp = (long) ( (long) cfg.sizeArea * 1024);
    netBytes = 0l;
    wildCard(getSize, ALL_FILES, "", WC_NO_COMMENTS);
    diskSpaceLeft(getdisk() + 'A', &temp3, &temp2);
    return (minimum(temp - netBytes, temp2));
}

/*
 * sysSendFiles()
 *
 * This is the system dep stuff for sending files.
 */
void sysSendFiles(struct fl_send *sendWhat)
{
    char   temp[100], *last;
    label  mask;

    strCpy(temp, sendWhat->snArea.naDirname);
    switch (fileType(sendWhat->snArea.naDirname, sendWhat->snArea.naDisk)) {
	case IS_DIR:
	    strCpy(mask, ALL_FILES);
	    break;
	case SINGLE_FILE:
	case AMB_FILE:
	    if ((last = strrchr(temp, '\\')) == NULL) {
		strCpy(mask, temp);
		temp[0] = 0;
	    }
	    else {
		strCpy(mask, last + 1);
		if (last != temp)
		    *last = 0;
		else
		    *(last + 1) = 0;
	    }
	    break;
	case NO_IDEA:
	default:
	    sprintf(temp, "Send File: Couldn't do anything with '%s'.",
					sendWhat->snArea.naDirname);
	    netResult(temp);
	    return;
    }
    if (!realSetSpace(toUpper(sendWhat->snArea.naDisk)-'A', temp)) {
	sprintf(msgBuf.mbtext, "Send file: Don't know what to do with %c:%s.",
				sendWhat->snArea.naDisk, temp);
	netResult(msgBuf.mbtext);
    }
    else
	wildCard(netSendFile, mask, "", WC_NO_COMMENTS);
    homeSpace();
}

char AuditBase[100];

/*
 * makeAuditName()
 *
 * This will make a file name for an audit file.
 */
void makeAuditName(char *logfn, char *str)
{
    sprintf(logfn, "%s%s", AuditBase, str);
}

/*
 * MakeDomainDirectory()
 *
 * This will make a domain directory.
 */
void DoDomainDirectory(int i, char kill)
{
    char num[8];
    DOMAIN_FILE name;

    sprintf(num, "%d", i);
    makeSysName(name, num, &cfg.domainArea);
    if (kill) rmdir(name);
    else      mkdir(name);
}

/*
 * Section 3.5. BAUD HANDLER:
 *    The code in here has to discover what baud rate the caller is at.
 * For some computers, this should be ridiculously easy.
 */

#define NO_GOOD		0
#define CR_CAUGHT	1
#define NET_CAUGHT	2
#define STROLL_CAUGHT	3
/*
 * check_CR()
 *
 * Checks for CRs from the data port for half a second.
 */
static check_CR()
{
    TimePacket ff;
    int c;

    setTimer(&ff);

    while (milliTimeSince(&ff) < 50) {
	if (MIReady())  {
	    switch ((c=Citinp())) {
	    case '\r':
		return CR_CAUGHT;
	    case 7:
		if (cfg.BoolFlags.netParticipant)
		    if (receive(1) == 13)
			if (receive(1) == 69)
			    return NET_CAUGHT;
	    case 68:
		if (receive(1) == 79)
		    if (receive(1) == 35)
			return STROLL_CAUGHT;
	    default: printf("%x\n", c);
	    }
	}
    }
    return NO_GOOD;
}

#define idStrings(i)   (cfg.codeBuf + cfg.DepData.pResults[i])

static int LastBaudIndex;

int BaudFlags;
/*
 * FindBaud()
 *
 * This finds the baud from sysop and user supplied data.
 */
UNS_32 FindBaud()
{
    char noGood = NO_GOOD;
    int  Time = 0;
    int  baudRunner;		/* Only try for 60 seconds      */
    extern FILE *netLog;
    extern UNS_32 BaudRate;
    extern UNS_32 intrates[];

    if ((LastBaudIndex = baudRunner = getModemId()) != ERROR) {
	setNetCallBaud(baudRunner);
	BaudRate = intrates[baudRunner] * 10l;
	pause((baudRunner != 4) ? 75 : 300);      /* To clear line noise */
	return BaudRate;
    }
    if (cfg.DepData.LockPort != -1) {
	return -1l;
    }
    while (MIReady())   Citinp();	/* Clear garbage	*/
    baudRunner = 0;
    while (gotCarrier() && noGood == NO_GOOD && Time < 120) {
	Time++;
	setNetCallBaud(baudRunner);
	noGood = check_CR();
	if (noGood == NO_GOOD)
	    baudRunner = (baudRunner + 1) % (cfg.sysBaud + 1);
    }
    if (noGood == NET_CAUGHT) {
	LastBaudIndex = baudRunner;
	netController(0, 0, NO_NETS, ANY_CALL, 0);
	return -1l;       /* pretend nothing happened */
    }
#ifdef STROLL_SUPPORTED
    if (noGood == STROLL_CAUGHT) {
	LastBaudIndex = baudRunner;
	StrollIt();
	return -1l;       /* pretend nothing happened */
    }
#endif
    BaudRate = intrates[baudRunner] * 10l;
    if (noGood == CR_CAUGHT) return BaudRate;
    return -1l;
}

#define BA_BUF_SIZE     50
static char IdBuffer[BA_BUF_SIZE];
static SListBase ReceivedResults = { NULL, FindStr, NULL, free, NULL };

void RunFax(void)
{
	extern char *FaxString;

	if (FaxString != NULL) {
		printf("Fax detected, running %s\n", FaxString);
		CitSystem(TRUE, "%s", FaxString);
		HangUp(TRUE);	/* yeah, TRUE */
	}
}

/*
 * getModemId()
 *
 * This tries to read the baud id from modem.
 */
int getModemId()
{
	NumToString *temp = NULL;
	char c;
	extern FILE *netLog;
	TimePacket ff;
	int i;
	void FindSpeed();

	RunListA(&ReceivedResults, FindSpeed, &temp);
	KillList(&ReceivedResults);

	if (temp != NULL) {
		BaudFlags = temp->num2;
		if (R_FAX == temp->num) RunFax();
		memset(IdBuffer, 0, sizeof IdBuffer);
		return temp->num;
	}

	setTimer(&ff);

	i = strlen(IdBuffer);
	while (timeSince(&ff) < 5l) {
		if (MIReady()) {
			if ((c = Citinp()) == '\r' || c == '\n') {
				IdBuffer[i] = 0;
				temp = SearchList(&ResList, IdBuffer);
				if (temp != NULL)
					switch (temp->num) {
					case R_300:
					case R_1200:
					case R_2400:
					case R_4800:
					case R_9600:
					case R_14400:
					case R_19200:
						BaudFlags = temp->num2;
						memset(IdBuffer, 0, sizeof IdBuffer);
						return temp->num;
					case R_FAX:
						RunFax();
						IdBuffer[0] = 0;
						memset(IdBuffer, 0, sizeof IdBuffer);
						return ERROR;
					}
				i = 0;	/* restart buffer filling */
			}
			else {
				if (c != '\n') {
		    			IdBuffer[i++] = c;
				}
				if (c == '\n' ||i >= BA_BUF_SIZE - 4) { /* Fudge factor */
					memset(IdBuffer, 0, sizeof IdBuffer);
		    			i = 0;
				}
			}
		}
	}
	IdBuffer[i] = 0;      /* debug for now */
	splitF(netLog, "autobaud failure, modem spat -%s-\n", IdBuffer);
	memset(IdBuffer, 0, sizeof IdBuffer);
	return ERROR;
}

/*
 * FindSpeed()
 *
 * This function helps check the list of received stuff from the modem
 * to see if any of it is any good for speed determination.
 */
static void FindSpeed(char *str, NumToString **temp)
{
	if (*temp == NULL) {
		*temp = SearchList(&ResList, str);
		if (*temp != NULL)
			switch ((*temp)->num) {
			case R_300:
			case R_1200:
			case R_2400:
			case R_4800:
			case R_9600:
			case R_14400:
			case R_19200:
			case R_FAX:
				return;
			}
		*temp = NULL;
	}
}

/*
 * CheckForFax()
 *
 * Since FaxModems don't set carrier high, we have to deal with this problem
 * that Citadel isn't set up for.  This function is called when Citadel detects
 * something on the serial port while carrier is not high.  If it's a FAX call,
 * this function is responsible for dealing with it.
 */
void CheckForFax()
{
	char c;
	NumToString *temp;

	if (MIReady()) {
		if ((c = Citinp()) == '\r' || c == '\n') {
			if ((temp = SearchList(&ResList, IdBuffer)) != NULL) {
				if (temp->num == R_FAX) {
					RunFax();
				}
				else {
					AddData(&ReceivedResults,
						strdup(IdBuffer), NULL, FALSE);
				}
			}
			memset(IdBuffer, 0, sizeof IdBuffer);
		}
		else if (c != '\n') {
			IdBuffer[strlen(IdBuffer)] = c;
			if (strlen(IdBuffer) >= BA_BUF_SIZE - 4) {
				memset(IdBuffer, 0, sizeof IdBuffer);
			}
		}
	}
}

/*
 * getNetBaud()
 *
 * This gets the baud of network caller -- refer to SysDep.doc
 */
char getNetBaud()
{
    extern UNS_32 intrates[];
    extern FILE *netLog;
    int		Time, baudRunner;
    extern UNS_32	BaudRate;
    char	laterMessage[100];
    char	found = FALSE, notFinished;
    extern char inNet;

	/* If anytime answer, then we already have baud rate */

    sprintf(laterMessage,
 "System will be in network mode for another %d minutes; please call back.\n",
							timeLeft());

    if (inNet == ANY_CALL || inNet == STROLL_CALL) {
	found      = TRUE;
	baudRunner = LastBaudIndex;
    }
    else if (GetFirst(&ResList)) {
	if ((baudRunner = getModemId()) != ERROR) {
	    found = TRUE;
	    setNetCallBaud(baudRunner);
	}
    }

    pause(50);	/* Pause a half second */

    if (found) {
	BaudRate = intrates[baudRunner] * 10l;
	for (Time = 0; gotCarrier() && Time < 20; Time++) {
	    if (check_for_init(FALSE)) return TRUE;
	    if (cfg.BoolFlags.debug) splitF(netLog, ".\n");
	}
	if (gotCarrier()) {
	    outFlag = IMPERVIOUS;
	    mPrintf(laterMessage);
	}
    }
    else {
	while (MIReady())   Citinp();	/* Clear garbage	*/

	for (Time = 0; gotCarrier() && Time < 20; Time++) {
	    for (notFinished = TRUE, baudRunner = 0; 
					gotCarrier() && notFinished;) {
		BaudRate = intrates[baudRunner]*10l;
		setNetCallBaud(baudRunner);
		if (check_for_init(FALSE)) return TRUE;  /* get connection */
		if (cfg.BoolFlags.debug) splitF(netLog, ".\n");
		notFinished = !(baudRunner == cfg.sysBaud);
		baudRunner++;
	    }
	}

	if (gotCarrier()) {
	    outFlag = IMPERVIOUS;
	    for (baudRunner = cfg.sysBaud; baudRunner > -1; baudRunner--) {
		setNetCallBaud(baudRunner);
		mPrintf(laterMessage);
	    }
	    outFlag = OUTOK;
	}
    }
    if (!gotCarrier()) splitF(netLog, "Lost carrier\n");
    killConnection("gnb");
    return FALSE;
}

/*
 * Section 3.3. CONSOLE HANDLING:
 *	These functions are responsible for handling console I/O.
 */

/*
 * mputChar()
 *
 * This puts stuff to the console.  Bell filtering is in vputch().
 */
void mputChar(char c)
{
    extern char ChatMode;

    if (c == '\0')
	return;
    if (!(whichIO == CONSOLE || onConsole) && !anyEcho)
	return;
    if (c != ESC &&
      (echo == BOTH || (whichIO == CONSOLE && (echo != NEITHER || echoChar)))) {
	if (!cfg.DepData.OldVideo) vputch(c);
	else	DoBdos(6, c);
	if (c == '\n')
	    mputChar('\r');
    }
}

/*
 * 3.11. File Comment handling
 *   These four functions handle file comments.  In DOS, there is no
 * built-in way to keep file comments; therefore, we keep a file in
 * each subdirectory named FILEDIR.TXT which contains the file comments.
 */


/*
 * StFileComSearch()
 *
 * Start File Comment search: see SYSDEP.DOC.
 */
int StFileComSearch()
{
    fileTags = fopen(flDir, READ_TEXT);
    tagFound = TRUE;
    msgBuf.mbtext[0] = 0;
    return TRUE;
}

/*
 * FindFileComment()
 *
 * This will find a file comment for file: see SYSDEP.DOC.
 */
int FindFileComment(char *fileName, char extraneous)
{
    char *c, Chatter = FALSE;
    int Last;

    if (fileTags == NULL) return FALSE;

    /* Check to see if already in buffer */
    if (specCmpU(fileName, msgBuf.mbtext) == SAMESTRING) return TRUE;

    /* 
     * No. If last search was successful, then we have to get the next
     * line right now.
     */
    if (tagFound) {
	do {
	    if ((c = fgets(msgBuf.mbtext, MAXTEXT-10, fileTags)) == NULL)
		break;
	    if (msgBuf.mbtext[0] == ';')
		if (extraneous) {
		    mFormat(msgBuf.mbtext + 1, oChar, doCR);
		    Last = strlen(msgBuf.mbtext + 1);	/* finicky check */
		    Chatter = TRUE;
		}
	} while (msgBuf.mbtext[0] == ';');
    }

    if (Chatter && Last > 1)/* need this for ";" stuff -- allows normal	*/
	doCR();		/* formatting without blank lines.		*/

    tagFound = FALSE;

    while (c != NULL && specCmpU(fileName, msgBuf.mbtext) > SAMESTRING) {
	do {
	    if ((c=fgets(msgBuf.mbtext, MAXTEXT-10, fileTags)) == NULL)
		break;
	    if (msgBuf.mbtext[0] == ';')
		if (extraneous) {
		    mFormat(msgBuf.mbtext + 1, oChar, doCR);
		    doCR();
		}
	} while (msgBuf.mbtext[0] == ';');
    }

    if (c != NULL)
	tagFound = (specCmpU(fileName, msgBuf.mbtext) == SAMESTRING);

    return tagFound;
}

/*
 * EndFileComment()
 *
 * This will end the session of reading file comments.
 */
void EndFileComment()
{
    if (fileTags != NULL) fclose(fileTags);
    fileTags = NULL;
}

/*
 * updFiletag()
 *
 * This function updates a file tag.
 */
void updFiletag(char *fileName, char *desc)
{
    FILE	*updFd, *temp;
    char	*line, *c, *l2, abort = FALSE;
    static char *tmpName = "a45u8a7.$$$",
		*format  = "%s %s\n";

    if ((updFd = fopen(flDir, READ_TEXT)) == NULL) {
	if ((updFd = fopen(flDir, WRITE_TEXT)) == NULL)
	    mPrintf("Unknown problem with filetagging!\n ");
	else {
	    fprintf(updFd, format, fileName, desc);
	    fclose(updFd);
	}
	return;
    }
    else {
	line = strdup(msgBuf.mbtext);
	l2 = strdup(desc);		/* necessary -- desc == mbtext! */
	temp = fopen(tmpName, WRITE_TEXT);
	msgBuf.mbtext[0] = 0;
	while ((c=fgets(msgBuf.mbtext, MAXTEXT, updFd)) != NULL) {
	    if (msgBuf.mbtext[0] != ';')
		if (specCmpU(fileName, msgBuf.mbtext) <= SAMESTRING) break;
	    if (fprintf(temp, "%s", msgBuf.mbtext) == EOF)
		abort = TRUE;
	}
	if (fprintf(temp, format, fileName, l2) == EOF)
	    abort = TRUE;

	if (c != NULL && specCmpU(fileName, msgBuf.mbtext) != SAMESTRING)
	    if (fprintf(temp, "%s", msgBuf.mbtext) == EOF)
		abort = TRUE;

	while ((c=fgets(msgBuf.mbtext, MAXTEXT, updFd)) != NULL)
	    if (fprintf(temp, "%s", msgBuf.mbtext) == EOF)
		abort = TRUE;

	fclose(updFd);
	fclose(temp);
	if (!abort) {
	    unlink(flDir);
	    rename(tmpName, flDir);
	}
	strCpy(msgBuf.mbtext, line);
	free(line);
	free(l2);
    }
}

#define FAX_DEBUG
#ifdef FAX_DEBUG
void AddFaxResults()
{
	void ShowFaxResult();

	RunList(&ResList, ShowFaxResult);
}

void ShowFaxResult(NumToString *data)
{
	if (data->num == R_FAX) {
		sprintf(lbyte(msgBuf.mbtext), "\"%s\"\n ", data->string);
	}
}
#endif
