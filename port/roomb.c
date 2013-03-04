/*
 *				roomb.c
 *
 * room code for Citadel bulletin board system
 */

/*
 *				History
 *
 * 87Mar30 HAW  Fix for carrier loss in renameRoom, Invite only room
 *		bug, add <L>ogin to novice menu for unlogged
 * 86Aug16 HAW  Kill history in here due to space problems.
 * 85Jan16 JLS  Fix getText so console starting CR creates blank msg.
 * 84Jun28 JLS  Enhancement: Creator of a room is listed in Aide>.
 * 84Apr04 HAW  Start upgrade to BDS 1.50a.
 * 83Feb26 CrT  bug in makeRoom when out of rooms fixed.
 * 83Feb26 CrT  matchString made caseless, normalizeString()
 * 83Feb26 CrT  "]" directory prompt, user name before prompts
 * 82Dec06 CrT  2.00 release.
 * 82Nov02 CrT  Cleanup prior to V1.2 mods.
 * 82Nov01 CrT  Proofread for CUG distribution.
 * 82Mar27 dvm  conversion to v. 1.4 begun
 * 82Mar25 dvm  conversion for TRS-80/Omikron test started
 * 81Dec21 CrT  Log file...
 * 81Dec20 CrT  Messages...
 * 81Dec19 CrT  Rooms seem to be working...
 * 81Dec12 CrT  Started.
 */

#define NET_INTERFACE

#include "ctdl.h"

/*
 *				Contents
 *
 *	CleanEnd()		cleans up end of msg
 *	editText()		handles the end-of-message-entry menu
 *	findRoom()		find a free room
 *	getNumber()		prompt user for a number, limited range
 *	getString()		read a string in from user
 *	getText()		reads a message in from user
 *	getYesNo()		prompts for a yes/no response
 *	givePrompt()		gives usual "THISROOM>" prompt
 *	indexRooms()		build RAM index to ctdlroom.sys
 *	initialArchive()	does initial archive of a room
 *	insertParagraph()	inserts paragraph into message
 *	makeRoom()		make new room via user dialogue
 *	matchString()		search for given string
 *	noteRoom()		enter room into RAM index
 *	renameRoom()		sysop special to rename rooms
 *	replaceString()		string-substitute for message entry
 *	searchForRoom()		auxilary to addToList()
 *
 *	# -- operating system dependent function.
 */

#define ROLL_OVER	"Rollover size in K (use 0 for no rollover)"

char *public_str  = " Public";
char *private_str = " Private";
char *perm_str	  = "Permanent";
char *temp_str	  = "Temporary";
char exChar	  = '?';
char *on	  = "on";
char *off	  = "off";
char *no	  = "no";
char *yes	  = "yes";
extern SListBase Arch_base;

extern aRoom	  roomBuf;	/* Room buffer */
extern rTable	  *roomTab;	/* RAM index   */
extern FILE	  *roomfl;	/* Room file descriptor */
extern CONFIG	  cfg;	/* Other variables */
extern MessageBuffer   msgBuf;	/* Message buffer */
extern MessageBuffer   tempMess;	/* For held messages */
extern logBuffer logBuf;	/* Person buffer */
extern logBuffer logTmp;	/* Person buffer */
extern NetBuffer netBuf;
extern SListBase  Moderators;
extern int  masterCount, thisRoom, thisLog;
extern char remoteSysop;
extern char outFlag;		/* Output flag */
extern char loggedIn;		/* Logged in? */
extern char haveCarrier;	/* Have carrier? */
extern char onConsole;		/* How about on Console? */
extern char whichIO;		/* Where is I/O? */
extern char *baseRoom;
extern char heldMess;
extern char echo;
extern char echoChar;
extern char *confirm;
extern int  thisNet;
extern char *WRITE_TEXT;
extern FILE *netLog;
extern SListBase DirBase;
extern int  CurLine;

char NetworkSelected;
/*
 * editText()
 *
 * This funcion handles the end-of-message-entry menu.
 *
 * return TRUE  to save message to disk,
 *	  FALSE to abort message, and
 *	  ERROR if user decides to continue
 */
static int editText(char *buf, int lim, char MsgEntryType, char *entry)
{
    extern char *ALL_LOCALS, *WRITE_LOCALS;
    char c, *oldRec;
    int  letter;
    char *OtherEdit[] = {
	"Abort\n", "Continue", "Replace string\n", "Print formatted\n",
	"Global Replace\n",
	" ", " ", " ", " ", " ", " ", " ", " ",
	" ", " ", " ", " ", " ", " ", " ", ""
    };
    static struct {
	char *Save;
	char *Menu;
	char *Print;
    } Displays[] = {
	{ "Saving message\n", "edit.mnu", NULL },
	{ "Saving file description\n", "descedit.mnu", "File description of" },
	{ "Saving information\n", "infoedit.mnu", "Information" },
	{ "Saving biography\n", "infoedit.mnu", "Biography" },
    };

    ExtraOption(OtherEdit, Displays[MsgEntryType].Save);

    if (MsgEntryType == MSG_ENTRY) {
	ExtraOption(OtherEdit, "Hold message for later\n");
	ExtraOption(OtherEdit, "Insert paragraph break\n");
	if (thisRoom == MAILROOM && loggedIn)
	    ExtraOption(OtherEdit, "Who else\n");
	if (NetValidate(FALSE) && (thisRoom == MAILROOM || 
							roomBuf.rbflags.SHARED))
	    ExtraOption(OtherEdit, "N");
    }
    else if (MsgEntryType == INFO_ENTRY || MsgEntryType == BIO_ENTRY) {
	ExtraOption(OtherEdit, "Insert paragraph break\n");
    }

    if (onConsole && cfg.BoolFlags.SysopEditor)
	ExtraOption(OtherEdit, "Outside Editor");

    OtherEditOptions(OtherEdit);

    RegisterThisMenu(Displays[MsgEntryType].Menu, OtherEdit);

    do {
	outFlag = IMPERVIOUS;
	doCR();
	if (MsgEntryType == MSG_ENTRY && !logBuf.lbflags.NoPrompt)
	    mPrintf("[%s] ", roomBuf.rbname);
	mPrintf("entry cmd: ");
	switch ((letter = GetMenuChar())) {
	case 'A':
	    if (strLen(buf) == 0 || getYesNo(confirm)) {
		return FALSE;
	    }
	    break;
	case 'C':
	    doCR();
	    mPrintf("...%s", CleanEnd(buf));
	    return ERROR;
	case 'I':
	    insertParagraph(buf, lim);
	    break;
	case 'W':
	    if (thisRoom == MAILROOM) {
		getList(OtherRecipients, "Other recipients", CC_SIZE, FALSE,
						ADD_TO_LIST | USE_CC);
		getList(OtherRecipients, "Users to take off Other recipients",
							CC_SIZE, FALSE,
						KILL_FROM_LIST | USE_CC);
	    }
	    break;
	case 'P':
	    outFlag = OUTOK;
	    PagingOn();
	    if (Displays[MsgEntryType].Print != NULL) {
		doCR();
		mPrintf("   %s %s", Displays[MsgEntryType].Print, entry);
		doCR();
		mFormat(buf, oChar, doCR);
		doCR();
	    }
	    else
		printMessage(TRUE, 0);
	    PagingOff();
	    break;
	case 'R':
	case 'G':
	    replaceString(buf, lim, (letter != 'R'));
	    break;
	case 'S':
	    if (MsgEntryType == MSG_ENTRY && roomBuf.rbflags.SHARED &&
		cfg.BoolFlags.netParticipant &&
		loggedIn &&
		strLen(msgBuf.mbaddr) == 0 && !NetworkSelected &&
		logBuf.lbflags.NET_PRIVS) {
		if (getYesNo("Save as net message"))
		    if (!netInfo(TRUE)) break;
	    }
	    if (MsgEntryType == MSG_ENTRY && roomBuf.rbflags.ANON && loggedIn) {
		if (!getYesNo("Save as anonymous message")) {
		    msgBuf.mbdate[0] = 0;
		    strcpy(msgBuf.mbauth, logBuf.lbname);
		}
	    }
	    return TRUE;
	case 'H':
	    if (heldMess) {
		mPrintf("Message already being held!\n ");
		break;
	    }
	    if (thisRoom != MAILROOM) /* 117.644 */
		msgBuf.mbaddr[0] = 0;	/* 117.638 && 117.640 */
	    mPrintf("Message held\n ");
	    MoveMsgBuffer(&tempMess, &msgBuf);
	    heldMess = TRUE;
	    return FALSE;
	case '?':
	    if ((MsgEntryType == MSG_ENTRY && roomBuf.rbflags.SHARED || thisRoom == MAILROOM)
		&& cfg.BoolFlags.netParticipant &&
		loggedIn &&
		logBuf.lbflags.NET_PRIVS)
		mPrintf(" <N>et toggle\n ");

	    if (cfg.BoolFlags.SysopEditor && onConsole)
		mPrintf("<O>utside Editor\n ");

	    if (MsgEntryType == MSG_ENTRY && thisRoom == MAILROOM && loggedIn)
		mPrintf("<W>ho else (Carbon Copies)\n ");

	    ShowOutsideEditors();

	    break;
	case 'O':
	    OutsideEditor();
	    break;
	case 'N':
	    NetworkSelected++;
	    if (msgBuf.mbaddr[0]) {
		c = msgBuf.mbaddr[0];	/* need to remember this */
		msgBuf.mbaddr[0] = 0;	/* zero for getRecipient */
		mPrintf("\bormal message\n ");
		oldRec = strdup(msgBuf.mbto);	/* getRec might* zero mbto */
		if (getRecipient()) {
		    msgBuf.mboname[0] = 0;
		    msgBuf.mbdomain[0] = 0;
		}
		else {
		    msgBuf.mbaddr[0] = c;  /* failed, restore address */
		    strcpy(msgBuf.mbto, oldRec);	/* restore recip */
		}
		free(oldRec);
	    }
	    else {
		mPrintf("\betwork message\n ");
		netInfo(TRUE);
	    }
	    break;
	default:
		RunRemoteEditor(letter);
	}
    } while (onLine());

    if (MsgEntryType == MSG_ENTRY && loggedIn)
	SaveInterrupted(&msgBuf);

    return FALSE;
}

char *NoCredit = "Sorry, you have no credit for the long distance net.\n ";
/*
 * OtherRecipients()
 *
 * This function handles adding or deleting Other Recipients.
 */
int OtherRecipients(char *name, int flags)
{
    label	  person;
    char	  buf[CC_SIZE], isdomain;
    char	  system[(2 * NAMESIZE) + 10], *domain;
    static char	  *nope = "'%s' not found.\n";
    int		  result, cost, auth_slot;
    SListBase	  *list;
    extern SListBase TempForward;

    if (flags & USE_CC) list = &msgBuf.mbCC;
    else list = &TempForward;

    result = SepNameSystem(name, person, system, &netBuf);
    strcpy(buf, name);
    if (result == IS_SYSTEM)
	sprintf(buf, "%s @%s", person, system);

    isdomain = (domain = strchr(system, '_')) != NULL;
    if (isdomain)
	domain += 2;		/* formats to "system _ domain", so we cheat */

    if (flags & KILL_FROM_LIST) {   /* Take it off the list */
	KillData(list, buf);
    }
    else {
	switch (result) {
	case BAD_FORMAT:
	    mPrintf(nope, name); break;
	case SYSTEM_IS_US:
	    break;		/* error message handled in SepNameSystem */
	case NO_SYSTEM:
	    mPrintf(nope, system); break;
	case IS_SYSTEM:
	    cost = (isdomain) ? FindCost(domain) : !netBuf.nbflags.local;
	    if (logBuf.credit < cost) {
		if (HalfSysop()) {
		    logBuf.credit += cost;
		}
		else {
		    mPrintf(NoCredit);
		    break;
		}
	    }
	    else if (!logBuf.lbflags.NET_PRIVS) {
		mPrintf("Sorry, you don't have network privileges.\n");
		break;
	    }
	    AddData(list, strdup(buf), NULL, TRUE);
	    break;
	case NOT_SYSTEM:
	    if ((cost = PersonExists(name)) == ERROR) {
		mPrintf(nope, name);
		break;
	    }
	    if (!AcceptableMail(thisLog, cost)) {
		mPrintf(nope, name);
		break;
	    }
	    if (flags & CHECK_AUTH) {
	        if ((auth_slot = PersonExists(msgBuf.mbauth)) != ERROR) {
		    if (!AcceptableMail(auth_slot, cost)) {
			mPrintf(nope, name);
			break;
		    }
		}
	    }
	    if (strCmpU(name, logBuf.lbname) == SAMESTRING)
		mPrintf("Can't send mail to yourself, silly!\n");
	    else if (!msgBuf.mbaddr[0] &&
				strCmpU(name, msgBuf.mbto) == SAMESTRING)
		mPrintf("%s is the main recipient, silly!\n", name);
	    else {
		AddData(list, strdup(name), NULL, TRUE);
	    }
	    break;
	}
    }
    return TRUE;
}

/*
 * SepNameSystem()
 *
 * This will parse an Other Recipient spec.
 */
char SepNameSystem(char *string, char *person, char *system, NetBuffer *buf)
{
    char  *c;
    label domain;
    char dup, work[150];		/* should be sufficient */
    int   slot;
    extern char *DupDomain;

    strcpy(work, string);

    if ((c = strchr(work, '@')) == NULL) {
	if (strLen(work) >= NAMESIZE) return BAD_FORMAT;
	strcpy(person, string);
	return NOT_SYSTEM;
    }

    *c++ = 0;

    NormStr(work);
    NormStr(c);

    if (strLen(c) >= NAMESIZE * 2 || strLen(work) >= NAMESIZE)
	return BAD_FORMAT;

    strcpy(system, c);
    strcpy(person, work);

    if (buf == NULL) return IS_SYSTEM;	/* very minor cheat - see CTDL.C */

    if ((slot = searchNameNet(c, buf)) != ERROR) { /* try secondary lists */
	strcpy(system, buf->netName);	/* get "real" name */
	if (buf->nbflags.local || buf->nbflags.RouteLock) {
	    return IS_SYSTEM;
	}
    }

    if (SystemInSecondary(c, domain, &dup)) {
	if (dup) {	/* oops */
	    if (slot == ERROR)
		mPrintf(DupDomain, c);
	    return (slot == ERROR) ? NO_SYSTEM : IS_SYSTEM;
	}
	if (strCmpU(domain, cfg.nodeDomain + cfg.codeBuf) == SAMESTRING &&
	    (strCmpU(c, cfg.nodeName + cfg.codeBuf) == SAMESTRING
		|| strCmpU(c, UseNetAlias(cfg.nodeName+cfg.codeBuf, TRUE))
							== SAMESTRING)) {
	    mPrintf("Hey, that's this system!\n ");
	    return SYSTEM_IS_US;
	}
	sprintf(system, "%s _ %s", c, domain);
	return IS_SYSTEM;
    }

    return (slot == ERROR) ? NO_SYSTEM : IS_SYSTEM;
}

/*
 * findRoom()
 *
 * This function finds and returns the # of a free room slot if possible,
 * else ERROR.
 */
int findRoom()
{
    int roomRover;

    for (roomRover = 0;  roomRover < MAXROOMS;  roomRover++) {
	if (roomTab[roomRover].rtflags.INUSE == 0) return roomRover;
    }
    return ERROR;
}

/*
 * getNumber()
 *
 * This prompts for a number in (bottom, top) range.
 */
long getNumber(char *prompt, long bottom, long top)
{
    long try;
    char numstring[NAMESIZE];

    do {
	getString(prompt, numstring, NAMESIZE, 0);
	try	= atol(numstring);
	if (try < bottom)  mPrintf("Sorry, must be at least %ld\n", bottom);
	if (try > top   )  mPrintf("Sorry, must be no more than %ld\n", top);
    } while ((try < bottom ||  try > top) && onLine());
    return  (long) try;
}

/*
 * getString()
 *
 * This gets a string from the user.
 */
int getString(char *prompt, char *buf, int lim, int Flags)
{
    int toReturn;
    extern int crtColumn;
    char oldEcho;

    outFlag = IMPERVIOUS;

    if (strLen(prompt) > 0) {
	doCR();
	mPrintf("Enter %s\n : ", prompt);
    }

    oldEcho = echo;
    if (Flags & NO_ECHO) {
	echo	 = NEITHER;
	echoChar = 'X';
    }

    outFlag   = OUTOK;
    Flags |= CR_ON_ABORT;
    if ((toReturn  = BlindString(buf, lim, Flags, iChar, oChar, 0)) == BACKED_OUT)
	return toReturn;
    echo	= oldEcho;
    crtColumn	= 1;
    return toReturn;
}

/*
 * BlindString()
 *
 * Gets a string blind to source and sink.
 */
int BlindString(char *buf, int lim, int Flags,
			int (*input)(void), void (*output)(char c), char Echo)
{
    int  c;
    int  i;

    i   = 0;
    while (
	c = (*input)(),
	c	!= NEWLINE &&
	c	!= '\r'
	&& i	<  lim
	&& onLine()
    ) {

	if (Echo) (*output)(c);
	/* handle delete chars: */
	if (c == BACKSPACE) {
	    (*output)(' ');
	    (*output)(BACKSPACE);
	    if (i > 0) i--;
	    else if (Flags & BS_VALID) {
		return BACKED_OUT;
	    }
	    else {
		(*output)(' ');
		(*output)(BELL);
	    }
	} else if (c) buf[i++] = c;

	if (i >= lim) {
	    (*output)(BELL);
	    (*output)(BACKSPACE); i--;
	}

	/* kludge to return immediately on single '?': */
	if ((Flags & QUEST_SPECIAL) && *buf == exChar)   {
	    if ((Flags & CR_ON_ABORT)) doCR();
	    /* i--; */
	    break;
	}
    }
    buf[i]  = '\0';
    return GOOD_SELECT;
}

/*
 * getText()
 *
 * This manages reading a message from the user.
 * Returns TRUE if user decides to save it, else FALSE (whether held or
 * aborted).
 */
char getText(int uploading)
{
    extern PROTO_TABLE Table[];
    extern char *R_SH_MARK, EndWithCR, *UploadLog;
    int  i, toReturn;

    /* msgBuf.mbtext[-1] = NEWLINE; */

    if (uploading == ASCII) {
	if (!expert) {
	    printHelp("entry.blb", HELP_SHORT | HELP_BITCH);
	    mPrintf("Enter message (end with empty line)");
	}
	outFlag = OUTOK;
	if (msgBuf.mbtext[0])
	    CleanEnd(msgBuf.mbtext);

	EndWithCR = FALSE;
	CurLine = 1;
	PagingOn();
	printMessage(TRUE, 0);
	PagingOff();
	EndWithCR = TRUE;
	outFlag = OUTOK;
    }
    else {
	if (!expert && InternalProtocol(uploading))
	    printHelp(Table[uploading].UpBlbName, HELP_SHORT | HELP_BITCH);
	if (!getYesNo("Ready for transfer"))
	    return FALSE;
	masterCount = 0;
	if (uploading <= TOP_PROTOCOL) {
	    if (Reception(uploading, putBufChar) != TRAN_SUCCESS)
		return FALSE;
	}
	else {
	    if (EatExtMessage(uploading) != TRAN_SUCCESS)
		return FALSE;
	    unlink(UploadLog);
	}
    }

    toReturn = GetBalance(uploading, msgBuf.mbtext, MAXTEXT, MSG_ENTRY, "");

    if (toReturn == TRUE) {		/* Filter null messages		*/
	toReturn = FALSE;
	for (i = 0; msgBuf.mbtext[i] != 0 && !toReturn; i++)
	    toReturn = (msgBuf.mbtext[i] > ' ' && msgBuf.mbtext[i] < 127);
    }
    return  toReturn;
}

/*
 * GetBalance()
 *
 * This function gets the balance of text for a message.
 */
char GetBalance(int uploading, char *buf, int size, char type, char *entry)
{
    char c, beeped = FALSE;
    int  i, toReturn;

    do {
	i = strLen(buf);
	if (uploading == ASCII)
	    while (
		!(
		    (c=iChar()) == NEWLINE   &&
		    (i == 0 || buf[i-1] == NEWLINE)
		)
		&& i < size - 1
		&& onLine()
	    ) {
		if (c != BACKSPACE) {
		    if (c == ESC) {
			if (iChar() == '[') {
			    iChar();
			    continue;	/* arrow key. */
			}
		    }
		    if (c != 0 && c != XOFF && c != XON) buf[i++]   = c;
		    if (i > size - 80 && !beeped) {
			beeped = TRUE;
			oChar(BELL);
		    }
		}
		else {
		    /* handle delete chars: */
		    oChar(' ');
		    oChar(BACKSPACE);
		    if (i > 0 && buf[i-1] != NEWLINE)   i--;
		    else				oChar(BELL);
		}

		buf[i] = 0;   /* null to terminate message	*/

		if (i == size - 1) {
		    mPrintf(" buffer overflow\n ");
		    while (receive(2) != ERROR)
			;
		}
	    }

	if (i == size - 1)
	    buf[--i] = 0;	/* fixes an odd buffer overflow problem */
				/* couldn't edit the message otherwise */

	toReturn = editText(buf, size - 1, type, entry);
	uploading = ASCII;
    } while ((toReturn == ERROR)  &&  onLine());

    return toReturn;
}

/*
 * getYesNo()
 *
 * This prompts for a yes/no response and gets it.
 */
char getYesNo(char *prompt)
{
    int  toReturn;
    extern char ConOnly;
    int (*input)(void);

    input = ConOnly ? getCh : iChar;

    for (doCR(), toReturn = ERROR; toReturn == ERROR && onLine(); ) {
	outFlag = IMPERVIOUS;
	mPrintf("%s? (Y/N): ", prompt);

	switch (toUpper((*input)())) {
    	case 'Y': toReturn	= TRUE ;	break;
    	case 'N': toReturn	= FALSE;	break;
	}

	doCR();
    }
    outFlag = OUTOK;
    return   toReturn;
}

/*
 * givePrompt()
 *
 * This function simply prints the usual "CURRENTROOM>" prompt -- not as simple
 * as it may seem.
 */
void givePrompt()
{
    outFlag = IMPERVIOUS;
    doCR();
    ScreenUser();
    if (!loggedIn)
	mPrintf("<L>ogin ");
    if (!expert) {
	mPrintf("%s%s<G>oto <U>ngoto <H>elp",
     (thisRoom == MAILROOM || loggedIn ||
			cfg.BoolFlags.unlogReadOk)  ? "<N>ew messages "   : "",
     (thisRoom == MAILROOM || loggedIn ||
			cfg.BoolFlags.unlogEnterOk) ? "<E>nter " : "");
	doCR();
    }

    if (roomBuf.rbflags.READ_ONLY) {
	mPrintf(" [ read only ]");
	doCR();
    }

    mPrintf("%s ", formRoom(thisRoom, FALSE, TRUE));

    if (strcmp(roomBuf.rbname, roomTab[thisRoom].rtname) != SAMESTRING) {
	printf("thisRoom=%d, rbname=-%s-, rtname=-%s-\n", thisRoom,
		roomBuf.rbname, roomTab[thisRoom].rtname);
	crashout("Dependent variables mismatch!");
    }
    outFlag = OUTOK;
}

/*
 * indexRooms()
 *
 * This function will try to find and free an empty room.
 */
void indexRooms()
{
    int  goodRoom, slot;

    for (slot = 0;  slot < MAXROOMS;  slot++) {
	if (roomTab[slot].rtflags.INUSE == 1) {
	    goodRoom = FALSE;
	    if (roomTab[slot].rtlastMessage > cfg.oldest ||
		roomTab[slot].rtflags.PERMROOM == 1) {
		goodRoom    = TRUE;
	    }

	    if (!goodRoom) {
		getRoom(slot);
		KillData(&DirBase, NtoStrInit(thisRoom, "", 0, TRUE));
		WriteAList(&DirBase, "ctdldir.sys", WrtNtoStr);
		KillInfo(roomTab[slot].rtname);
		roomBuf.rbflags.INUSE    = 0;
		putRoom(slot);
		strcat(msgBuf.mbtext, roomBuf.rbname);
		strcat(msgBuf.mbtext, "> ");
		noteRoom();
	    }
	}
    }
}

static char *NoRoom = "No room.\n ";
/*
 * insertParagraph()
 *
 * This inserts a paragraph (CR/Space) into a message.
 * (By Jay Johnson of The Phoenix)
 */
void insertParagraph(char *buf, int lim)
{
    char oldString[2*SECTSIZE];
    char *loc, *textEnd;
    char *pc;
    int length;

    for (textEnd = buf, length = 0; *textEnd; length++, textEnd++);
    if (lim - length < 3) {
	mPrintf(NoRoom);
	return;
    }
    getString("string", oldString, (2*SECTSIZE), 0);
    if ((loc=matchString(buf, oldString, textEnd)) == NULL) {
	mPrintf("?not found.\n ");
	return;
    }
    for (pc=textEnd; pc>=loc; pc--) {
	*(pc+2) = *pc;
    }
    *loc++ = '\n';
    *loc = ' ';
}

/*
 * makeRoom()
 *
 * This function constructs a new room via dialogue with user.
 */
void makeRoom()
{
    extern struct floor  *FloorTab;
    label nm, oldName;
    int   CurrentFloor, oldRoom;

    CurrentFloor = thisFloor;
    oldRoom = thisRoom;

    strcpy(oldName, roomBuf.rbname);
    if ((thisRoom = findRoom()) == ERROR) {
	indexRooms();   /* try and reclaim an empty room	*/
	if ((thisRoom = findRoom()) == ERROR) {
	    mPrintf(NoRoom);
	/* may have reclaimed old room, so: */
	    if (roomExists(oldName) == ERROR)   strcpy(oldName, baseRoom);
	    getRoom(roomExists(oldName));
	    return;
	}
    }

    getNormStr("name for new room", nm, NAMESIZE, 0);
    if (strLen(nm) == 0) {
	if (roomExists(oldName) == ERROR)   strcpy(oldName, baseRoom);
	getRoom(roomExists(oldName));
	return ;
    }

    if (roomExists(nm) >= 0) {
	mPrintf(" A '%s' already exists.\n", nm);
	/* may have reclaimed old room, so: */
	if (roomExists(oldName) == ERROR)   strcpy(oldName, baseRoom);
	getRoom(roomExists(oldName));
	return;
    }
    if (!expert)   printHelp("newroom.blb", HELP_SHORT | HELP_BITCH);

    zero_struct(roomBuf.rbflags);
    roomBuf.rbflags.INUSE    = TRUE;

    if (getYesNo(" Make room public"))	roomBuf.rbflags.PUBLIC = TRUE;
    else				roomBuf.rbflags.PUBLIC = FALSE;

    mPrintf("'%s', a %s room", nm,
	roomBuf.rbflags.PUBLIC == 1  ?  "public"  :  "private"
    );

    if(!getYesNo("Install it")) {
	/* may have reclaimed old room, so: */
	if (roomExists(oldName) == ERROR)   strcpy(oldName, baseRoom);
	getRoom(roomExists(oldName));
	return;
    }
    else if (roomExists(oldName) == ERROR) oldRoom = -1;	/* in case */

    /* update lastMessage for current room: */

    logBuf.lastvisit[oldRoom]	= cfg.newest;

    /* now initialize new room */

    strcpy(roomBuf.rbname, nm);
    memset(roomBuf.msg, 0, MSG_BULK);   /* mark all slots empty */

    roomBuf.rbgen = (roomTab[thisRoom].rtgen + 1) % MAXGEN;
    roomBuf.rbFlIndex = CurrentFloor;

    KillData(&Moderators, NtoStrInit(thisRoom, "", 0, TRUE));

    if (cfg.BoolFlags.AutoModerate) {
	AddData(&Moderators, NtoStrInit(thisRoom, logBuf.lbname, 0, FALSE),
								NULL, TRUE);
    }

    WriteAList(&Moderators, "ctdlmodr.sys", WrtNtoStr);

    KillData(&Arch_base, NtoStrInit(thisRoom, "", 0, TRUE));
    WriteAList(&Arch_base, "ctdlarch.sys", WrtArchRec);

    noteRoom();			/* index new room	*/

    putRoom(thisRoom);

    ZeroMsgBuffer(&msgBuf);
    mPrintf("You may now enter the initial Information on %s\n ",
		roomBuf.rbname);
    if (!cfg.BoolFlags.NoInfo)
	EditInfo();	/* creator gets to set information */

    msgBuf.mboname[0] = 0;	/* icky kludge */
    if (strLen(msgBuf.mbtext) != 0 &&
				getYesNo("Make Information into a message"))
	procMessage(ASCII, FALSE);

    /* update logBuf: */
    logBuf.lbrgen[thisRoom]	= roomBuf.rbgen;
    logBuf.lastvisit[thisRoom]	= 0l;
    ZeroMsgBuffer(&msgBuf);
    sprintf(msgBuf.mbtext, "%s created by %s on floor %s",	
		formRoom(thisRoom, FALSE, FALSE), logBuf.lbname,
 			FloorTab[CurrentFloor].FlName);
    aideMessage(NULL, FALSE);
    roomTab[thisRoom].rtlastNetAll = 0l;
    roomTab[thisRoom].rtlastNetBB = 0l;
    if (oldRoom != -1)
	UngotoMaintain(oldRoom);
}

/*
 * matchString()
 *
 * This searches for match to given string.  Runs backward through buffer so
 * we get most recent error first.  Returns loc of match, else NULL.
 */
char *matchString(char *buf, char *pattern, char *bufEnd)
{
    char *loc, *pc1, *pc2;
    char foundIt;

    for (loc = bufEnd, foundIt = FALSE;  !foundIt && --loc >= buf;) {
	for (pc1 = pattern, pc2 = loc,  foundIt = TRUE ;  *pc1 && foundIt;) {
	    if (! (toUpper(*pc1++) == toUpper(*pc2++)))   foundIt = FALSE;
	}
    }

    return   foundIt  ?  loc  :  NULL;
}

/*
 * getNormStr()
 *
 * This function gets a string and deletes leading & trailing blanks etc.
 */
int getNormStr(char *prompt, char *s, int size, int Flags)
{
    int toReturn;

    if ((toReturn = getString(prompt, s, size, Flags)) != BACKED_OUT)
	if (*s != exChar) NormStr(s);

    return toReturn;
}

/*
 * renameRoom()
 *
 * This is a sysop special fn -- it handles all room editing.
 * Returns:	TRUE on success else FALSE.
 */
char renameRoom()
{
    void UpdateSharedForNewGen(AN_UNSIGNED gen);
    int c, r;
    char *dft, *buffer, wasRO, wasShared, wasAnon, *save, workbuf[200];
    extern char *APrivateRoom;
    char doAideMessage = 0;	/* Counter to determine if aide msg needed */
    extern char *WRITE_TEXT;
    extern FILE *upfd;
    char *RoomEditOpts[] = {
	"X\beXit room editing\n", "Name change\n", "Temporary room\n",
	"Private setting\n", "Lure users to room\n", "Only Invitational\n",
	"Innominate status\n", "Values\n", "Unforgettable\n",
	"Withdraw Invitations\n", "Read only\n",
	" ", " ", " ", " ", " ", " ", " ", " ", " ", ""
    };

    if (				/* clearer than "thisRoom <= AIDEROOM"*/
	thisRoom == LOBBY
	||
	thisRoom == MAILROOM
	||
	thisRoom == AIDEROOM
    ) {
	mPrintf("? -- may not edit this room.\n ");
	return FALSE;
    }

    wasShared = roomBuf.rbflags.SHARED;
    wasAnon = roomBuf.rbflags.ANON;
    wasRO = roomBuf.rbflags.READ_ONLY;

    ZeroMsgBuffer(&msgBuf);
    sprintf(msgBuf.mbtext, "%s, formerly ", formRoom(thisRoom, FALSE, FALSE));
    formatSummary(lbyte(msgBuf.mbtext), FALSE);
    buffer = strdup(msgBuf.mbtext);

    if (!cfg.BoolFlags.NoInfo) ExtraOption(RoomEditOpts, "Edit information\n");

    if (HalfSysop()) {
	ExtraOption(RoomEditOpts, "Archive status\n");
	ExtraOption(RoomEditOpts, "Directory status\n");
	if (cfg.BoolFlags.netParticipant) {
	    ExtraOption(RoomEditOpts, "Backbone setting");
	    ExtraOption(RoomEditOpts, "Shared room\n");
	}
	if (cfg.BoolFlags.netParticipant && roomBuf.rbflags.ISDIR) {
	    ExtraOption(RoomEditOpts, "Z\bNet Downloadable\n");
	}
    }

    if (aide)
	ExtraOption(RoomEditOpts, "Moderator setting\n");

    RegisterThisMenu(HalfSysop() ? "rooms.mnu" : "rooma.mnu", RoomEditOpts);

    c = 0;	/* Init */
    while (c != 'X' && onLine()) {
	mPrintf("\n Room edit cmd: ");
	c = GetMenuChar();
	switch (c) {
    	case 'X':
		break;
    	case 'A':
		doAideMessage++;
		roomBuf.rbflags.ARCHIVE = getYesNo("Activate room archiving");
		if (roomBuf.rbflags.ARCHIVE) {
		    getString("filename", workbuf, (sizeof workbuf) - 1, 0);
		    if (strLen(workbuf) == 0) {
			roomBuf.rbflags.ARCHIVE = FALSE;
			break;
		    }

		    c = (int) getNumber(ROLL_OVER, -1, 32000);
		    
		    AddData(&Arch_base, NtoStrInit(thisRoom, workbuf, c, FALSE),
					NULL, TRUE /* kill duplicates */);

		    WriteAList(&Arch_base, "ctdlarch.sys", WrtArchRec);
		    if (getYesNo("Do an initial archive"))
			initialArchive(workbuf);
		}
		break;
    	case 'N':
		getNormStr("new room name", workbuf, NAMESIZE, 0);
		r = roomExists(workbuf);
		if (r >= 0  &&  r != thisRoom) {
		    mPrintf("A %s exists already!\n", workbuf);
		    break;
		} else {
		    ChangeInfoName(workbuf);
		    strcpy(roomBuf.rbname, workbuf);  /* also in room itself  */
		    doAideMessage++;
		}
		if (cfg.BoolFlags.NoInfo ||
			!getYesNo("Care to edit the info of this room")) break;
	case 'E':
		save = strdup(msgBuf.mbtext);
		EditInfo();
		RegisterThisMenu(SomeSysop() ? "rooms.mnu" : "rooma.mnu",
								RoomEditOpts);
		strcpy(msgBuf.mbtext, save);
		free(save);
		break;
    	case 'B':
		if (!roomBuf.rbflags.SHARED) {
		    mPrintf("This room is not shared!\n ");
		    break;
		}
		doCR();
		getList(knownHosts,
			"Systems that you will be a Backbone for",
				NAMESIZE, FALSE, BACKBONE);
		getList(knownHosts,
			"Systems that should be returned to Peon status",
				NAMESIZE, FALSE, PEON);
		break;
    	case 'M':
		if (WhoIsModerator(workbuf)) {
		    if (strLen(workbuf))
			AddData(&Moderators, NtoStrInit(thisRoom, workbuf, 0,
			FALSE), NULL, TRUE);
		    else
			KillData(&Moderators, NtoStrInit(thisRoom, "", 0, TRUE));
		}
		WriteAList(&Moderators, "ctdlmodr.sys", WrtNtoStr);
		doAideMessage++;
		break;
    	case 'D':
		doAideMessage++;
		roomBuf.rbflags.ISDIR = getYesNo("Activate directory");
		if (roomBuf.rbflags.ISDIR) {
		    if ((dft = FindDirName(thisRoom)) != NULL)
			mPrintf("Old directory was %s\n ", dft);
		    if (!getXInternal(NO_MENU, "directory", workbuf,
				sizeof workbuf, dft, dft))
			roomBuf.rbflags.ISDIR = FALSE;
		    else if ((roomBuf.rbflags.ISDIR = ValidArea(workbuf))) {
			roomBuf.rbflags.PERMROOM = TRUE;
			roomBuf.rbflags.UPLOAD=getYesNo("Are uploads allowed");
			roomBuf.rbflags.DOWNLOAD=
					getYesNo("Are downloads allowed");
			if (!roomBuf.rbflags.UPLOAD &&
					!roomBuf.rbflags.DOWNLOAD)
			    mPrintf("You're strange.\n ");
			AddData(&DirBase, NtoStrInit(thisRoom, workbuf,
			     0, FALSE), NULL, /* Kill duplicates */ TRUE);
			WriteAList(&DirBase, "ctdldir.sys", WrtNtoStr);
		    }
		    else break;
		}
		break;
    	case 'T':
		if (roomBuf.rbflags.ISDIR)
		    mPrintf("Directory rooms are always permanent\n ");
		else {
		    roomBuf.rbflags.PERMROOM = !getYesNo("Room is temporary");
		    doAideMessage++;
		}
		break;
    	case 'U':
		doAideMessage++;
		roomBuf.rbflags.UNFORGETTABLE=getYesNo("Room is unforgettable");
		break;
    	case 'P':
		doAideMessage++;
		roomBuf.rbflags.PUBLIC = !getYesNo("Make room private");
		if (!roomBuf.rbflags.PUBLIC) {
		    if (getYesNo("Cause non-aide users to forget room")) {
			if (roomBuf.rbflags.SHARED) {
				UpdateSharedForNewGen((roomBuf.rbgen +1) % MAXGEN);
			}
			roomBuf.rbgen = (roomBuf.rbgen +1) % MAXGEN;
			logBuf.lbrgen[thisRoom] = roomBuf.rbgen;
			roomTab[thisRoom].rtgen = roomBuf.rbgen;
		    }
		}
		else roomBuf.rbflags.INVITE = FALSE;
		break;
    	case 'S':
		roomBuf.rbflags.SHARED = getYesNo("Network (shared) room");
		if (roomBuf.rbflags.SHARED) {
		    if (!wasShared) {
			roomBuf.rbflags.PERMROOM = TRUE;
			doAideMessage++;

				/* cosmetic bug fix */
		    	roomTab[thisRoom].rtflags.SHARED = TRUE;
		    }
		    getList(knownHosts, wasShared ?
				"Systems to add to network list for this room" :
				"Systems to network this room with", NAMESIZE,
								FALSE, PEON);
		    if (wasShared)
			getList(killFromList,
				"Systems to take off network list", NAMESIZE,
								FALSE, 0);
		    roomBuf.rbflags.AUTO_NET =
			getYesNo("Should messages automatically be networked");
		    if (roomBuf.rbflags.AUTO_NET)
			roomBuf.rbflags.ALL_NET =
			    getYesNo("Even for users without net privileges");
		}
		else {
		    if (wasShared) doAideMessage++;
		    if (!roomBuf.rbflags.ISDIR)
			roomBuf.rbflags.PERMROOM = FALSE;
		    KillSharedRoom(thisRoom);
		}
		break;
    	case 'L':
		getList(makeKnown, "Users to be invited", NAMESIZE, FALSE, 0);
		break;
    	case 'R':
		if ((roomBuf.rbflags.READ_ONLY = getYesNo("Read-Only room"))) {
		    getList(WritePrivs, "Users needing write privileges",
							NAMESIZE, FALSE, TRUE);
		    getList(WritePrivs, "Users losing write privileges",
							NAMESIZE, FALSE, FALSE);
		}
		if (roomBuf.rbflags.READ_ONLY != wasRO)
		    doAideMessage++;
		break;
    	case 'O':
		doAideMessage++;
		if ((roomBuf.rbflags.INVITE = getYesNo("Invitation only room")))
		{
		    roomBuf.rbflags.PUBLIC = FALSE;
		    if (getYesNo("Cause non-aide users to forget room")) {
			if (roomBuf.rbflags.SHARED) {
				UpdateSharedForNewGen((roomBuf.rbgen +1) % MAXGEN);
			}
			roomBuf.rbgen = (roomBuf.rbgen +1) % MAXGEN;
			logBuf.lbrgen[thisRoom] = roomBuf.rbgen;
			roomTab[thisRoom].rtgen = roomBuf.rbgen;
		    }
		    getList(makeKnown, "Users to be invited", NAMESIZE,FALSE,0);
		}
		break;
    	case 'I':
		roomBuf.rbflags.ANON = getYesNo("Anonymous room");
		if (roomBuf.rbflags.ANON != wasAnon)
		    doAideMessage++;
		break;
    	case 'V':
		mPrintf("%s is %s.", roomBuf.rbname,
				formatSummary(msgBuf.mbtext, TRUE));
		break;
    	case 'W':
		getList(makeUnknown, "Users to be kicked out", NAMESIZE, FALSE, 0);
		break;
    	case 'Z':
		doAideMessage++;
		roomBuf.rbflags.NO_NET_DOWNLOAD =
		    !getYesNo("Room accessible to network");
		break;
	}
    }

    noteRoom();
    putRoom(thisRoom);

    UpdateSharedRooms();

    if (doAideMessage) {
	ZeroMsgBuffer(&msgBuf);
	sprintf(msgBuf.mbtext, "%s, has been edited to %s, ", buffer,
				formRoom(thisRoom, FALSE, FALSE));
	formatSummary(lbyte(msgBuf.mbtext), FALSE);
	sprintf(lbyte(msgBuf.mbtext), ", by %s.", logBuf.lbname);

	aideMessage(NULL, FALSE);
    }
    free(buffer);
    return TRUE;
}

/*
 * UpdateSharedForNewGen()
 *
 * When a shared room is made private or invite only, the generation number
 * used to key sharedness is changed. Therefore, we need to update the shared
 * room database as appropriate.
 */
static void UpdateSharedForNewGen(AN_UNSIGNED gen)
{
	extern SListBase SharedRooms;
	void NewGen();

	RunListA(&SharedRooms, NewGen, &gen);
}

static void NewGen(SharedRoomData *room, AN_UNSIGNED *gen)
{
printf("NG: room=%p, room->room=%p\n", room, room->room);
	if (isSharedRoom(room->room) && netRoomSlot(room->room) == thisRoom) {
printf("NG: Updating %d\n", room->slot);
		room->srd_flags |= SRD_DIRTY;
		room->room->srgen = (*gen) + 0x8000;
	}
}

/*
 * WhoIsModerator()
 *
 * This handles adding moderating-type people.
 */
char WhoIsModerator(char *buf)
{
    if (!getXString("moderator", buf, NAMESIZE, "no moderator", ""))
	return FALSE;

    if (strLen(buf) != 0)
	if (findPerson(buf, &logTmp) == ERROR) {
	    mPrintf("No '%s' found\n ", buf);
	    return FALSE;
	}

    return TRUE;
}

/*
 * formatSummary()
 *
 * This is responsible for formatting a summary of the current room.
 */
char *formatSummary(char *buffer, char NotFinal)
{
    char *c;
    int size;

    sprintf(buffer, "a%s, ",
		roomBuf.rbflags.INVITE ? "n Invitation only" :
		roomBuf.rbflags.PUBLIC ? public_str : private_str);

    sprintf(lbyte(buffer), "%s",
		roomBuf.rbflags.PERMROOM ? perm_str : temp_str);

    if (roomBuf.rbflags.ARCHIVE) {
	sprintf(lbyte(buffer), ", Archived ('%s', ", 
					AskForNSMap(&Arch_base, thisRoom));
	if ((size = GetArchSize(thisRoom)) == 0)
	    strcat(buffer, "no rollover)");
	else
	    sprintf(lbyte(buffer), "rollover at %dK)", size);
    }


    if (roomBuf.rbflags.ANON)
	strcat(buffer, ", Anonymous");

    if (roomBuf.rbflags.READ_ONLY)
	strcat(buffer, ", Read-Only");

    if (roomBuf.rbflags.SHARED) {
	strcat(buffer, ", Shared");
	if (roomBuf.rbflags.AUTO_NET) {
	    strcat(buffer, " (autonet for ");
	    strcat(buffer, roomBuf.rbflags.ALL_NET ?	"all users)" :
							"net-priv users)");
	}
    }

    if (roomBuf.rbflags.UNFORGETTABLE) {
	strcat(buffer, ", Unforgettable");
    }

    if (roomBuf.rbflags.ISDIR) {
	strcat(buffer, ", Directory (");
	if (NotFinal || !(roomBuf.rbflags.INVITE || !roomBuf.rbflags.PUBLIC)) {
	    if (FindDirName(thisRoom) != NULL)
		strcat(buffer, FindDirName(thisRoom));
	    strcat(buffer, ", ");
	}

	if (roomBuf.rbflags.UPLOAD)
	    strcat(buffer, "uploads, ");

	if (roomBuf.rbflags.DOWNLOAD)
	    strcat(buffer, "downloads, ");

	sprintf(lbyte(buffer), "%snet downloadable",
	    roomBuf.rbflags.NO_NET_DOWNLOAD ? "not " : "");

	strcat(buffer, ")");
    }
    strcat(buffer, " room");

    if (strLen(c = AskForNSMap(&Moderators, thisRoom)) != 0)
	sprintf(lbyte(buffer), " (Moderator is %s)", c);

    if (roomBuf.rbflags.SHARED && NotFinal)
	ParticipatingNodes(buffer);

    return buffer;
}

MenuId GetListId;
/*
 * getList()
 *
 * This will get a list of names and blindly process them.
 *
 * NB: The return value is the number of times the user-function returned
 * TRUE.
 */
int getList(int (*fn)(char *data, int g), char *prompt, int size,
							char Sysop, int arg)
{
    char *buffer, work[70];
    int count = 0, result;

    buffer = GetDynamic(size + 1);
    GetListId = NO_MENU;
    sprintf(work, "%s (Empty line to end)\n", prompt);
    if (Sysop) {
	GetListId = SysopContinual("", work, strLen(work) + 5, 12);
    }
    else {
	doCR();
	mPrintf("%s", work);
    }
    do {
	if (Sysop) SysopPrintf(GetListId, " : ");
	else mPrintf(" : ");
	SysopContinualString(GetListId, "", buffer, size, 0);
	if (strLen(buffer) != 0) {
	    result = (*fn)(buffer, arg);
	    if (!result) break;
	    if (result == TRUE) count++;
	}
    } while (strLen(buffer) != 0 && onLine());
    if (Sysop) SysopCloseContinual(GetListId);
    free(buffer);
    return count;
}

/*
 * replaceString()
 *
 * This function corrects typos in message entry.
 */
#define REPLACE_SIZE    2000
void replaceString(char *buf, int lim, char Global)
{
    char oldString[2*SECTSIZE];
    char *newString;	/* Eliminate stack overflows in Turbo c */
    char *loc, *oldloc, *textEnd;
    char *pc;
    int  incr, length, oldLen, newLen, maxLen;
						/* find terminal null */
    textEnd = lbyte(buf);
    length = strLen(buf);

    getString("string", oldString, (2*SECTSIZE), 0);
    if ((oldLen = strLen(oldString)) == 0) return;

    if ((oldloc = loc = matchString(buf, oldString, textEnd)) == NULL) {
	mPrintf("?not found.\n ");
	return;
    }

    /* so we never have too long of a replacement string (unless Global R.) */
    maxLen = minimum(REPLACE_SIZE, (MAXTEXT - (length - oldLen)));

    newString = GetDynamic(REPLACE_SIZE);
    getString("replacement", newString, maxLen, 0);
    newLen = strLen(newString);
    do {
	textEnd = lbyte(buf);
	length = textEnd - buf;
	if ( (newLen - oldLen)  >=  lim - length) {
	    mPrintf("?Overflow!\n ");
	    free(newString);
	    return;
	}

	/* delete old string: */
	for (pc=loc,
	    incr=strLen(oldString);
	    *pc=*(pc+incr);	/* Compiler generates a warning for this line */
	    pc++);
	textEnd -= incr;

    /* make room for new string: */
	for (pc=textEnd, incr=strLen(newString);  pc>=loc;  pc--) {
	    *(pc+incr) = *pc;
	}

	/* insert new string: */
	for (pc=newString;  *pc;  *loc++ = *pc++);

	if (Global) oldloc = loc = matchString(buf, oldString, oldloc);
    } while (Global && oldloc != NULL);

    free(newString);
}

/*
 * initialArchive()
 *
 * This function is responsible for doing an initial archive of a room.
 */
void initialArchive(char *fn)
{
    char *TmpMsg, *realfn;

    TmpMsg = strdup(msgBuf.mbtext);
    realfn = GetDynamic(strLen(fn) + 15);
    TranslateFilename(realfn, fn);
    mPrintf("Doing the initial archive\n ");
    msgToDisk(realfn, TRUE, 0l, 0l, 0);
    strcpy(msgBuf.mbtext, TmpMsg);
    free(TmpMsg);
    free(realfn);
}

/*
 * knownHosts()
 *
 * This function handles setting systems as sharing the room currently in
 * roomBuf.
 *
 * NB: The caller to this function should call UpdateSharedRooms() after using
 * this function.
 */
static int knownHosts(char *name, int ShType)
{
    SharedRoomData *room;

    if (CmnNetList(name, &room, ERROR, "does not share this room with you")
								== ERROR) {
	return TRUE;
    }

    if (room == NULL)
	if ((room = ListAsShared(name)) == NULL)
	    return TRUE;

    SetMode(room->room->mode, ShType);
    room->srd_flags |= SRD_DIRTY;

    return TRUE;
}

/*
 * ListAsShared()
 *
 * This function will find an empty share slot.
 */
SharedRoomData *ListAsShared(char *name)
{
    SharedRoomData *room;

    room = NewSharedRoom();	/* get a record ... */
    room->room->srslot   = thisRoom;
    room->room->sr_flags    = 0;
    room->room->lastMess = cfg.newest;
    room->room->srgen    = roomBuf.rbgen + (unsigned) 0x8000;
    room->room->netSlot  = thisNet;
    room->room->mode     = 0;
    SetMode(room->room->mode, PEON);

    return room;
}

/*
 * searchForRoom()
 *
 * This function checks to see if the current room is in the current node's
 * room sharing list.
 */
SharedRoomData *searchForRoom(char *name)
{
    RoomSearch data;

    strcpy(data.Room, name);
    EachSharedRoom(thisNet, IsRoomRoutable, NULL, &data);
    if (data.reason == FOUND) return data.room;
    return NULL;
}

/*
 * getXString()
 */
char getXString(char *prompt, char *target, int targetSize, char *CR_str,
								char *dft)
{
    return getXInternal(NO_MENU, prompt, target, targetSize, CR_str, dft);
}

/*
 * getXInternal()
 * 
 * This is a sophisticated GetString().  Will return TRUE and FALSE -- read
 * the code to figure it out.
 */
char getXInternal(MenuId id, char *prompt, char *target, int targetSize,
						char *CR_str, char *dft)
{
    char ourPrompt[100], result, xpage;

    xpage = Pageable;
    Pageable = FALSE;
    sprintf(ourPrompt, "%s (", prompt);
    if (CR_str != NULL && strLen(CR_str) != 0)
	sprintf(lbyte(ourPrompt), "C/R = '%s', ", CR_str);

    strcat(ourPrompt, "ESCape to abort)");
    exChar = ESC;
    result = SysopContinualString(id, ourPrompt, target, targetSize, QUEST_SPECIAL);
    exChar = '?';
    Pageable = xpage;
    CurLine = 1;
    if (!result || !onLine()) return FALSE;	/* Lost carrier */
    if (target[0] == ESC) return FALSE;
    if (CR_str == NULL && target[0] == 0) return FALSE;
    else if (target[0] == 0) strcpy(target, dft);
    return TRUE;
}

/*
 * makeKnown()
 *
 * This makes a user knowledgeable about this room.
 */
int makeKnown(char *user, int arg)
{
    return doMakeWork(user, roomBuf.rbgen);
}

/*
 * makeUnknown()
 *
 * This makes a user forget about this room.
 */
int makeUnknown(char *user, int arg)
{
    return doMakeWork(user, (roomBuf.rbgen + (MAXGEN-1)) % MAXGEN);
}

/*
 * doMakeWork()
 *
 * This is a worker function.
 */
int doMakeWork(char *user, int val)
{
    int	target;

    if ((target = findPerson(user, &logTmp)) == ERROR)
	mPrintf("'%s' not found\n ", user);
    else {
	logTmp.lbrgen[thisRoom] = val;
	putLog(&logTmp, target);
    }
    return TRUE;
}

/*
 * killFromList()
 *
 * Kills systems from sharing a room.
 */
static int killFromList(char *sysName, int arg)
{
    int slot;
    SharedRoomData *room;

    if ((slot=CmnNetList(sysName, &room, TRUE,
			"does not net this room with you")) == ERROR)
	return TRUE;

    if (room != NULL) {
	/* room->room->srgen = 0; */
	room->room->sr_flags |= SR_NOTINUSE;
	room->srd_flags |= SRD_DIRTY;
    }

    putNet(slot, &netBuf);
    return TRUE;
}

/*
 * CmnNetList()
 *
 * This does general work on net stuff.
 */
int CmnNetList(char *name, SharedRoomData **room, char ShouldBeThere,
								char *errstr)
{
    extern int thisNet;

    /* This will do required getNet() if it returns true */
    if (!ReqNodeName("", name, NULL, RNN_ONCE, &netBuf))
	return ERROR;

    *room = searchForRoom(roomBuf.rbname);
    if ((*room == NULL && ShouldBeThere == TRUE) ||
	(*room != NULL && ShouldBeThere == FALSE)) {
	mPrintf("%s %s.", name, errstr);
	doCR();
	return ERROR;
    }
    return thisNet;
}

/*
 * WritePrivs()
 *
 * Here we give or take away write privs.
 */
int WritePrivs(char *user, int DoWritePrivs)
{
    return doMakeWork(user, (DoWritePrivs) ?
		(((roomBuf.rbgen + RO_OFFSET)) % MAXGEN) : roomBuf.rbgen);
}

