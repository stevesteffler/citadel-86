/*
 *				msg.c
 *
 * Message handling for Citadel bulletin board system.
 */

/*
 *				history
 *
 * 86Aug15 HAW  Large chunk of History deleted due to space problems.
 * 84Mar29 HAW Start upgrade to BDS C 1.50a, identify _spr problem.
 * 83Mar03 CrT & SB   Various bug fixes...
 * 83Feb27 CrT  Save private mail for sender as well as recipient.
 * 83Feb23	Various.  transmitFile() won't drop first char on WC...
 * 82Dec06 CrT  2.00 release.
 * 82Nov05 CrT  Stream retrieval.  Handles messages longer than MAXTEXT.
 * 82Nov04 CrT  Revised disk format implemented.
 * 82Nov03 CrT  Individual history begun.  General cleanup.
 */

#define MSG_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"

/* #define TEST_SYS */

/*
 *				contents
 *
 *	AddNetMail()		manage adding mail to a net system
 *	aideMessage()		saves auto message in Aide>
 *	canRespond()		respond on the net checker
 *	CheckForwarding()	forward mail to another system?
 *	dGetWord()		reads a word off disk
 *	doActualWrite()		to allow two message files for bkp
 *	doFlush()		writes out to specified msg file
 *	DoRespond()		respond to mail.
 *	fakeFullCase()		converts uppercase message to mixed case
 *	flushMsgBuf()		wraps up message-to-disk store
 *	getRecipient()		get recipient for the message
 *	hldMessage()		handles held messages
 *	mAbort()		checks for user abort of typeout
 *	makeMessage()		menu-level message-entry routine
 *	mPeek()			sysop debugging tool--shows ctdlmsg.sys
 *	msgToDisk()		puts a message to the given disk file
 *	noteMessage()		enter message into current room
 *	noteAMessage()		noteMessage() local
 *	printMessage()		prints a message on modem & console
 *	pullIt()		sysop special message-removal routine
 *	putLong()		puts a long integer to file
 *	putMessage()		write message to disk
 *	putMsgChar()		writes successive message chars to disk
 *	replyMessage()		reply to a Mail> message
 *	showMessages()		menu-level show-roomful-of-messages fn
 *	ShowReply()		reply tracing function
 */

extern FILE	    *msgfl;    /* file descriptor for the msg file	*/
extern FILE	    *msgfl2;   /* disk based backup msg file		*/

static char pullMessage = FALSE;/* true to pull current message*/
char	    journalMessage = FALSE;

SECTOR_ID   pulledMLoc;/* loc of pulled message		*/

MSG_NUMBER  pulledMId = 0l;    /* id of message to be pulled   */

label	    oldTarget;
char	    jrnlFile[100] = "";
char	    *NoNetRoomPrivs = "You do not have net privileges";
char	    outFlag = OUTOK;   /* will be one of the above     */
char	    heldMess;
char	    PrintBanner = FALSE;
SListBase   SysList = { NULL, ChkCC, NULL, free, strdup };
SListBase   FwdVortex = { NULL, ChkCC, NULL, free, NULL };
char	    EndWithCR = TRUE;
int	    AnonMsgCount;
int	    IckyCount, IckyLevel=1000;
char	    DisVandals;
static char ReturnAddress[(2 * NAMESIZE) + 10];
static char ArchiveMail;

extern char EOP;
static char ReverseMessage;
static char MsgStreamEnter = FALSE;
char Showing  = WHATEVER;
char DiskHeld;

extern MessageBuffer   msgBuf;	/* The message buffer   */
extern MessageBuffer   tempMess;	/* For held messages	*/
extern struct mBuf mFile1, mFile2;
extern CONFIG    cfg;		/* Configuration variables	*/
extern logBuffer logBuf;	/* Buffer for the pippuls	*/
extern logBuffer logTmp;	/* Buffer for the pippuls	*/
extern aRoom     roomBuf;	/* Room buffer			*/
extern rTable    *roomTab;
extern UNS_16	 *RoomMsgCount;
extern NetBuffer netBuf, netTemp;

extern FILE	 *upfd;

extern int	 thisRoom;	/* Current room		*/
extern int	 thisNet;	/* Current node in use	  */
extern int	 thisLog;	/* Current log position	 */
extern int	 outPut;
extern NetTable  *netTab;

extern char	 *strFile;

extern char	 exChar;
extern char	 echo;	/* Output flag	*/
extern char	 echoChar;
extern char	 loggedIn;	 /* Logged in flag	*/
extern char	 whichIO;	 /* Who gets output?	*/
extern char	 prevChar;	 /* Output's evil purposes	 */
extern char	 inNet;
extern int	 TransProtocol;  /* Flag	*/
extern char	 haveCarrier;    /* Flag	*/
extern char	 onConsole;	 /* Flag	*/
extern char	 remoteSysop;
extern FILE	 *netLog;
extern char	 *ALL_LOCALS, *WRITE_ANY;
extern int	 CurLine;

static void LocalForwarding(int slot, label name, logBuffer *workBuf);

/*
 * aideMessage()
 *
 * This function saves an auto message in Aide>.
 */
void aideMessage(char *name, char noteDeletedMessage)
{
    int ourRoom, target;

    /* message is already set up in msgBuf.mbtext */
    putRoom(ourRoom = thisRoom);

    if (name == NULL || (target = roomExists(name)) == ERROR)
	target = AIDEROOM;

    getRoom(target);

    strCpy(msgBuf.mbauth, "Citadel");
    msgBuf.mbto[0]    = '\0';
    msgBuf.mboname[0] = '\0';
    putMessage(&logBuf, 0);

    if (noteDeletedMessage)   {
	noteAMessage(roomBuf.msg, MSGSPERRM, pulledMId, pulledMLoc);
    }

    putRoom(target);
    noteRoom();
    getRoom(ourRoom);
}

/*
 * canRespond()
 *
 * Can we set up an auto-response on the net?  This includes domain mail.
 */
char canRespond()
{
	int   cost, result;
	char  dup;
	label temp;
	label domain;

	if (inNet != NON_NET) {
		return FALSE;
	}

	ReturnAddress[0] = 0;

	if (msgBuf.mborig[0] == 0 &&	/* i.e. is local mail	   */
		msgBuf.mboname[0] == 0) {
		return TRUE;
	}

	if (!logBuf.lbflags.NET_PRIVS) {
		return FALSE;
	}

	normId(msgBuf.mborig, temp);
	domain[0] = 0;
	if ((result = searchNet(temp, &netBuf)) == ERROR ||
		    (!netBuf.nbflags.local && !netBuf.nbflags.RouteLock)) {
		if (msgBuf.mbdomain[0] != 0) {
			strCpy(domain, msgBuf.mbdomain);
		}
		else if (!SystemInSecondary(msgBuf.mboname, domain, &dup) || dup) {
			if (result == ERROR) {
				return FALSE;
			}
		}
	}

	if (strlen(domain) != 0) {
		sprintf(ReturnAddress, "%s _ %s", msgBuf.mboname, domain);
	}
	else {
		strCpy(ReturnAddress, netBuf.netName);
	}

	if (domain[0] == 0 && netBuf.nbflags.local) {
		return TRUE;
	}

	if (domain[0] == 0) {
		if (msgBuf.mbdomain[0] != 0) {
			cost = FindCost(msgBuf.mbdomain);
		}
		else {
			cost = !netBuf.nbflags.local;
		}
	}
	else cost = FindCost(domain);

	if (logBuf.credit >= cost) {
		return TRUE;
	}

	/* Sysop always has enough credits kludge */
	if (HalfSysop()) {
		logBuf.credit += cost;
		return TRUE;
	}

	mPrintf("\n Not enough LD credit.\n ");

	return FALSE;
}

/*
 * deleteMessage()
 *
 * This function deletes a message for pullIt().
 */
char deleteMessage(int m)
{
    char auth[MB_AUTH];

    /* record vital statistics for possible insertion elsewhere: */
    DelMsg(TRUE, m);
    strCpy(auth, msgBuf.mbauth);

    ZeroMsgBuffer(&msgBuf);
    /* note in Aide>: */
    sprintf(msgBuf.mbtext, "Following message from %s deleted by %s:",
   		(auth[0]) ? auth : "<anonymous>", logBuf.lbname);
    aideMessage(NULL, /* noteDeletedMessage== */ TRUE);
    return TRUE;
}

/*
 * DelMsg()
 *
 * This does actual work of deleting a msg from current room.
 */
void DelMsg(char killit, int m)
{
    int i;

    pulledMLoc = roomBuf.msg[m].rbmsgLoc;
#ifdef NORMAL_MESSAGES
    pulledMId  = roomBuf.msg[m].rbmsgNo ;
#else
    pulledMId  = (roomBuf.msg[m].rbmsgNo & S_MSG_MASK);
#endif

    if (thisRoom == AIDEROOM || !killit)   return ;

    /* return emptied slot: */
    for (i = m;  i > 0;  i--) {
	roomBuf.msg[i].rbmsgLoc	 = roomBuf.msg[i - 1].rbmsgLoc;
	roomBuf.msg[i].rbmsgNo	 = roomBuf.msg[i - 1].rbmsgNo ;
    }
    roomBuf.msg[0].rbmsgNo   = 0l;	 /* mark new slot at end as free */
    roomBuf.msg[0].rbmsgLoc  = 0;	 /* mark new slot at end as free */

    /* store revised room to disk before we forget...   */
    noteRoom();
    putRoom(thisRoom);
}

/*
 * dGetWord()
 *
 * This function fetches one word from current message, off disk.  It returns
 * TRUE if more words follow, else FALSE.
 */
char dGetWord(char *dest, int lim)
{
    int  c;

    --lim;	 /* play it safe */

    /* pick up any leading blanks: */
    for (c = getMsgChar(); (c == '\n' || c == ' ')  &&  c && lim;
							c = getMsgChar()) {
	if (lim) { *dest++ = c;   lim--; }
    }

    /* step through word: */
    for (; c != '\n' && c != ' ' && c && lim;   c = getMsgChar()) {
		/* eliminate annoying bells */
	if (lim && c != BELL) { *dest++ = c;   lim--; }
    }

    if (*(dest - 1) != '\n')
    /* trailing blanks: */
    for (;   c == ' ' && c && lim;   c = getMsgChar()) {
	if (lim) { *dest++ = c;   lim--; }
    }

    if (c)  unGetMsgChar(c);    /* took one too many    */

    *dest = '\0';		/* tie off string	 */

    return  c;
}

/*
 * doActualWrite()
 *
 * This is used to help mirror a message on both disk and in RAM disk (two
 * message files).
 */
char doActualWrite(FILE *whichmsg, struct mBuf *mFile, char c)
{
    MSG_NUMBER  temp;
    int		toReturn = 0;

    if (mFile->sectBuf[mFile->thisChar] == 0xFF)  {
	/* obliterating a msg   */
	toReturn = 1;
    }

    mFile->sectBuf[mFile->thisChar]   = c;

    mFile->thisChar    = ++mFile->thisChar % MSG_SECT_SIZE;

    if (mFile->thisChar == 0) { /* time to write sector out and get next: */
	temp = mFile->thisSector;
	temp *= MSG_SECT_SIZE;
	fseek(whichmsg, temp, 0);
	crypte(mFile->sectBuf, MSG_SECT_SIZE, 0);
	if (fwrite(mFile->sectBuf, MSG_SECT_SIZE, 1, whichmsg) != 1) {
	    crashout("?putMsgChar-write fail");
	}

	mFile->thisSector = ++mFile->thisSector % cfg.maxMSector;
	temp = mFile->thisSector;
	temp *= MSG_SECT_SIZE;
	fseek(whichmsg, temp, 0);
	if (fread(mFile->sectBuf, MSG_SECT_SIZE, 1, whichmsg) != 1) {
	    crashout("?putMsgChar-read fail");
	}
	crypte(mFile->sectBuf, MSG_SECT_SIZE, 0);
    }
    return  toReturn;
}

/*
 * doFlush()
 *
 * This does actual write for specified msg file.
 */
void doFlush(FILE *whichmsg, struct mBuf *mFile)
{
    long int s;

    s = mFile->thisSector;
    s *= MSG_SECT_SIZE;
    fseek(whichmsg, s, 0);
    crypte(mFile->sectBuf, MSG_SECT_SIZE, 0);
    if (fwrite(mFile->sectBuf, MSG_SECT_SIZE, 1, whichmsg) != 1) {
	crashout("?ctdlmsg.sys write fail");
    }
    crypte(mFile->sectBuf, MSG_SECT_SIZE, 0);
    fflush(whichmsg);
}

SListBase TempForward = { NULL, ChkCC, NULL, free, NULL };
/*
 * DoRespond()
 *
 * Does the user want to respond to mail or even skip the mail?
 */
int DoRespond(SECTOR_ID loc, MSG_NUMBER msgNo)
{
    int toReturn = -2;

    for (doCR(); onLine() && toReturn == -2; ) {
	outFlag = IMPERVIOUS;
	CurLine = 1;
	mPrintf("respond? (Y/N/Skip/Forward): ");
	switch (toUpper(iChar())) {
	    case 'Y': toReturn = TRUE; break;
	    case 'N': toReturn = FALSE; break;
	    case 'S': mPrintf("kip message"); toReturn = ERROR; break;
	    case 'F':
		mPrintf("orward");
		getList(OtherRecipients, "Forward To", CC_SIZE, FALSE,
				CHECK_AUTH | ADD_TO_LIST | USE_OVERRIDES);
		if (GetFirst(&TempForward) != NULL) {
		    if (findMessage(loc, msgNo, TRUE)) {
			getMsgStr(getMsgChar, msgBuf.mbtext, MAXTEXT);
			strCpy(msgBuf.mbMsgStat, "F'WD");
			putMessage(NULL,
				FORWARD_MAIL | SKIP_AUTHOR | SKIP_RECIPIENT);
		    }
		}
		KillList(&TempForward);
		break;
	}
	doCR();
    }
    outFlag = OUTOK;
    return toReturn;
}


/*
 * fakeFullCase()
 *
 * This function converts a message in uppercase-only to a reasonable mix.  It
 * can't possibly make matters worse...
 *
 * Algorithm: First alphabetic after a period is uppercase, all others are
 * lowercase, excepting pronoun "I" is a special case.  We assume an imaginary
 * period preceding the text.
 */
void fakeFullCase(char *text)
{
    char *c;
    char lastWasPeriod;
    char state;

    for(lastWasPeriod=TRUE, c=text;   *c;  c++) {
	if (
	    *c != '.'
	    &&
	    *c != '?'
	    &&
	    *c != '!'
	) {
	    if (isAlpha(*c)) {
		if (lastWasPeriod)	*c	 = toUpper(*c);
		else if (*c != 'l')	*c	 = toLower(*c);
		lastWasPeriod   = FALSE;
	    }
	} else {
	    lastWasPeriod	 = TRUE ;
	}
    }

    /* little state machine to search for ' i ': */
#define NUTHIN	  0
#define FIRSTBLANK	 1
#define BLANKI	  2
    for (state=NUTHIN, c=text;  *c;  c++) {
	switch (state) {
	case NUTHIN:
	    if (isSpace(*c))    state   = FIRSTBLANK;
	    else		state   = NUTHIN    ;
	    break;
	case FIRSTBLANK:
	    if (*c == 'i')	 state   = BLANKI    ;
	    else		state   = NUTHIN    ;
	    break;
	case BLANKI:
	    if (isSpace(*c))    state   = FIRSTBLANK;
	    else		state   = NUTHIN    ;

	    if (!isAlpha(*c))   *(c-1)  = 'I';
	    break;
	}
    }
}

/*
 * flushMsgBuf()
 *
 * This function wraps up writing a message to disk, takes into account 2nd
 * msg file if necessary.
 */
void flushMsgBuf()
{
    doFlush(msgfl, &mFile1);
    if (cfg.BoolFlags.mirror)
	doFlush(msgfl2, &mFile2);
}

/*
 * mAbort()
 *
 * This function returns TRUE if the user has aborted typeout.
 *
 * Globals modified:	 outFlag
 */
char mAbort()
{
    char c, toReturn = FALSE, oldEcho;

    /* Check for abort/pause from user */
    if (outFlag == IMPERVIOUS || outFlag == NET_CALL || outPut == DISK) {
	;
    }
    else if (!BBSCharReady()) {
	if (haveCarrier && !gotCarrier()) {
	    modIn();	    /* Let modIn() report the problem	 */
	    toReturn	= TRUE;
	}
	else if (!onConsole && KBReady() && !PrintBanner) {
	    if (!SurreptitiousChar(getCh())) {
		outFlag     = OUTSKIP;
		toReturn    = TRUE;
	    }
	}
	else toReturn	= FALSE;
    } else {
	oldEcho  = echo;
	echo     = NEITHER;
	echoChar = 0;
	c = toUpper(modIn());   /* avoid the filter */
	switch (c) {
	case XOFF:
	    while (iChar() != XON && (gotCarrier() || onConsole))
		;
	    break;
	case 'P':				   /*  pause:	*/
		do {
	    		toReturn = HandleControl(FALSE);
		} while (toReturn == MORE_UNDECIDED);
		switch (toReturn) {
		case MORE_INSIGNIFICANT:
			toReturn = FALSE;
			break;
		case MORE_FLOW_CONTROL:
			toReturn = TRUE;
			break;
		case MORE_ONE:
			toReturn = ERROR;
			break;
		default:
			printf("BUG!");
			break;
		}
		break;
	case 'J':				   /* jump paragraph:*/
	    if (outFlag != NO_CANCEL) {
		outFlag     = OUTPARAGRAPH;
		toReturn    = FALSE;
	    }
	    break;
	case 'N':				   /* next:	  */
	    if (outFlag != NO_CANCEL) {
		outFlag     = OUTNEXT;
		toReturn    = TRUE;
	    }
	    break;
	case 'S':				   /* skip:	  */
	    if (outFlag != NO_CANCEL) {
		outFlag     = OUTSKIP;
		toReturn    = TRUE;
	    }
	    break;
	case 'R':
	    if (Showing == MSGS) {
		pause(50);
		ReverseMessage = TRUE;
	    }
	    break;
	case 7: /* anytime net indicator */
	case 68: /* stroll indicator */
	    if (PrintBanner &&
			((c == 7 && cfg.BoolFlags.netParticipant) ||
			  c == 68)) {
		if (c == 7) {
		    if (CheckForSpecial(13, 69)) {
			outFlag  = NET_CALL;
			toReturn = TRUE;
		    }
		}
		else {
		    if (CheckForSpecial(79, 35)) {
			outFlag  = STROLL_DETECTED;
			toReturn = TRUE;
		    }
		}
		break;
	    }
	default:
	    break;
	}
	echo    = oldEcho;
    }
    return toReturn;
}

/*
 * HandleControl()
 *
 * This function returns TRUE if the user has aborted typeout.
 *
 * Globals modified:	 outFlag
 *
 * This has been split out so both Pause and More can access it.
 */
char HandleControl(char doq)
{
	extern SListBase Moderators;
	char c;
	extern char Continuous;

	c = modIn();			    /* wait to resume */
	if ( toUpper(c) == 'D' && Showing == MSGS &&
	     (aide || (strCmpU(logBuf.lbname, AskForNSMap(&Moderators, 
					     thisRoom)) == SAMESTRING
	     && strlen(logBuf.lbname) != 0)))
		pullMessage = TRUE;
	else if (	/* 119.675 */
		(Showing == MSGS || Showing == DIR_LISTING)
				&& toUpper(c) == 'J' && HalfSysop())
		journalMessage = TRUE;

	/*
	 * We do things this way to avoid getting caught in a
	 * recursive trap which I don't really care to trace down
	 * at the moment involving mPrintf accidentally "calling"
	 * message entry routines when it really shouldn't.  Not
	 * only can that get messy, we could also run into some stack
	 * overflow problems.  So instead we'll use this "kludge"
	 * which may actually yield a better behavior for us -- it
	 * lets us reprint the interrupted message after message
	 * composition.  Note we allow this option in Mail but it acts
	 * entirely differently -- see showMessage().  Also note we track
	 * READ-ONLY rooms here, too, rather than in showMessage().
	 */
	else if (toUpper(c) == 'E' && Showing == MSGS && HasWritePrivs()) {
		MsgStreamEnter = TRUE;
		outFlag     = OUTSKIP;
		return MORE_FLOW_CONTROL;
	}
	else if (toUpper(c) == 'C') {
		Continuous = TRUE;
	}
	else if (c == '?') {
		printHelp("more.blb",   HELP_NOT_PAGEABLE |
					HELP_NO_LINKAGE |
					HELP_BITCH |
					HELP_NO_HELP);
		outFlag = OUTOK;	/* if user stopped more.blb... */
		ShoveCR();
		/*
		 * I could recurse this.  However, that leaves a remote but
		 * but real hole in the stability of the program -- a user with
		 * a lot of patience could eventually force us into brain
		 * damage.
		 *
		 * A goto is an alternative but repulsive.
		 *
		 * So we return a special value instead and let the caller
		 * loop, if they want to.
		 */
		return MORE_UNDECIDED;
	}
	else if (doq) {
		switch (toUpper(c)) {
		case 'S':
		case 'Q':
			return MORE_FLOW_CONTROL;
		case '\r':
		case '\n':
			return MORE_ONE;
		case 'N':		   /* next:	  */
			if (outFlag != NO_CANCEL) {
				outFlag     = OUTNEXT;
			}
			break;
		case 'R':
			if (Showing == MSGS) {
				ReverseMessage = TRUE;
			}
		}
	}
	return MORE_INSIGNIFICANT;
}

/*
 * CheckForSpecial()
 *
 * This checks to see if a special call is incoming.  This can be a network
 * call or a stroll call (not implemented, really).
 */
char CheckForSpecial(int second, int third)
{
    if (receive(1) == second)
	if (receive(1) == third)
	    return TRUE;
    return FALSE;
}

/*
 * getRecipient()
 *
 * This gets the recipient for the message (Mail> only, of course).
 */
char getRecipient()
{
	label person;
	int slot;

	if (thisRoom != MAILROOM)  {
		msgBuf.mbto[0] = 0;	     /* Zero recipient	 */
		return TRUE;
	}

	if (msgBuf.mbto[0] == 0) {
		if (!loggedIn || (!aide && cfg.BoolFlags.noMail) ||
						logBuf.lbflags.TWIT) {
			strCpy(msgBuf.mbto, "Sysop");
			mPrintf(" (private mail to 'sysop')\n ");
			return TRUE;
		}

		getNormStr("recipient", msgBuf.mbto, sizeof msgBuf.mbto, 0);
		if (strlen(msgBuf.mbto) == 0) return FALSE;
	}

	switch (SepNameSystem(msgBuf.mbto, person, msgBuf.mbaddr, &netBuf)) {
	case IS_SYSTEM:
		if (!NetValidate(TRUE) || !netInfo(FALSE)) return FALSE;
		strCpy(msgBuf.mbto, person);
		/* strCpy(msgBuf.mbaddr, netBuf.netName); */
	case NOT_SYSTEM:
		break;
	default:
		mPrintf("Couldn't find target system.\n ");
	case SYSTEM_IS_US:
		return FALSE;
	}

	if (!msgBuf.mbaddr[0]) {
		if (strCmpU(msgBuf.mbto, logBuf.lbname) == SAMESTRING) {
			mPrintf("Can't send mail to yourself, silly!");
			return FALSE;
		}
		if ((slot = PersonExists(msgBuf.mbto)) == ERROR)   {
			mPrintf("No '%s' known", msgBuf.mbto);
			msgBuf.mbto[0] = 0;	     /* Zero recipient	 */
			return FALSE;
		}
		if (!AcceptableMail(thisLog, slot)) {
			mPrintf("No '%s' known", msgBuf.mbto);
			msgBuf.mbto[0] = 0;	     /* Zero recipient	 */
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * replyMessage()
 *
 * This function will get a reply to a Mail> message.
 */
char replyMessage(MSG_NUMBER msgNo, SECTOR_ID Loc)
{
    char who[MB_AUTH];
    char other[O_NET_PATH_SIZE];

    if (heldMess) {
	if (strCmpU(msgBuf.mbauth, tempMess.mbto) == SAMESTRING) {
	    return hldMessage(TRUE);
	}
    }
    strCpy(who, msgBuf.mbauth);
    strCpy(other, msgBuf.mbOther);
    ZeroMsgBuffer(&msgBuf);
    strCpy(msgBuf.mbto, who);
    strCpy(msgBuf.mbaddr, ReturnAddress);
    sprintf(msgBuf.mbreply, "%u:%lu", Loc, msgNo);	/* back ptr */
    if (strlen(other) != 0) {
	mPrintf("%s address is '%s'.", netBuf.netName, other);
	if (!getYesNo("ok")) {
	    sprintf(other, "%s address", netBuf.netName);
	    getNormStr(other, msgBuf.mbOther, O_NET_PATH_SIZE, 0);
	}
	else
	    strCpy(msgBuf.mbOther, other);
    }
    return (procMessage(ASCII, TRUE) == TRUE);
}

/*
 * hldMessage()
 *
 * This function handles held messages
 * TRUE indicates a message was added to the room
 * FALSE indicates room not disturbed (either msg was re-held or aborted)
 */
char hldMessage(char IsReply)
{
    int result;

    if (!heldMess) {
	mPrintf(" \n No message in the Hold buffer!\007\n ");
	return FALSE;
    }
    heldMess = FALSE;
    MoveMsgBuffer(&msgBuf, &tempMess);
    ZeroMsgBuffer(&tempMess);

    if (DiskHeld) {
	DiskHeld = FALSE;
	if ((result = roomExists(msgBuf.mbroom)) != ERROR) {
	    if (KnownRoom(result)) {
		mPrintf("\n Moving to %s.\n ", msgBuf.mbroom);
		getRoom(result);
		setUp(FALSE);
		if (thisRoom == MAILROOM)
		    echo = CALLER;
	    }
	}
    }

    if (roomTab[thisRoom].rtflags.SHARED == 0)
	msgBuf.mboname[0] = 0;
    else if (loggedIn && roomBuf.rbflags.SHARED &&
	     roomBuf.rbflags.AUTO_NET &&
	     (roomBuf.rbflags.ALL_NET || logBuf.lbflags.NET_PRIVS))
	netInfo(TRUE);

	/*
	 * this indicates the user did a .eh in Mail> and
	 * failed to type in a valid user name.  This saves the
	 * message back to the held buffer, rather than losing
	 * it (a nettlesome behavior).
	 */
    if ((result = procMessage(ASCII, IsReply)) == ERROR) {
	MoveMsgBuffer(&tempMess, &msgBuf);
	heldMess = TRUE;
	return FALSE;		/* indicate what happened */
    }

    return result;
}

/*
 * makeMessage()
 *
 * This is a menu-level routine to enter a message.
 *
 * Return: TRUE if message saved else FALSE.
 */
int makeMessage(char uploading)
{
    if (!loggedIn && AnonMsgCount > 1)
	return FALSE;

    if (loggedIn && roomBuf.rbflags.SHARED &&
	    roomBuf.rbflags.AUTO_NET &&
	    (roomBuf.rbflags.ALL_NET || logBuf.lbflags.NET_PRIVS))
	return netMessage(uploading);
    
    ZeroMsgBuffer(&msgBuf);
    return procMessage(uploading, FALSE);
}

/*
 * idiotMessage()
 *
 * This checks for idiocy by the user.
 */
char idiotMessage()
{
    int base, rover;

    if (DisVandals || loggedIn || thisRoom != MAILROOM)
	return FALSE;

    for (base = 0; msgBuf.mbtext[base]; base++) {
	for (rover = 0; rover < IDIOT_TRIGGER; rover++) {
	    if (msgBuf.mbtext[base] != msgBuf.mbtext[base + rover] ||
			msgBuf.mbtext[base] == ' ')
		break;
	}
	if (rover == IDIOT_TRIGGER) return TRUE;	/* Jackass caught! */
    }
    if (cfg.AnonMailLength > 0 && strlen(msgBuf.mbtext) > cfg.AnonMailLength &&
					 !onConsole) {
	HangUp(TRUE);
	sprintf(tempMess.mbtext, "   %s @ %s\n%s\n", formDate(), Current_Time(),
					msgBuf.mbtext);
	CallMsg("anonmail", tempMess.mbtext);
	strCpy(msgBuf.mbtext, "Indecently long anonymous Mail has been stored in ANONMAIL.");
	aideMessage(NULL,FALSE);
	return TRUE;
    }
	
    return FALSE;
}

/*
 * procMessage
 *
 * This is a menu-level routine to enter a message.
 */
int procMessage(char uploading, char IsReply)
{
	extern char NetworkSelected;

	PagingOff();
	if (!HasWritePrivs()) {
		mPrintf("This is a Read Only room.");
		return FALSE;
	}

	if (loggedIn)
		strCpy(msgBuf.mbauth,(roomBuf.rbflags.ANON) ? "":logBuf.lbname);

	strCpy(msgBuf.mbroom, roomBuf.rbname);
	strCpy(msgBuf.mbdate, (roomBuf.rbflags.ANON) ? "****" :  formDate());

	if (!getRecipient()) {
		return ERROR;
	}

	NetworkSelected = 0;
	if (getText(uploading) == TRUE) {
		if (!getRecipient()) {
			mPrintf("Unexpected internal error, please report it!\n ");
			return FALSE;
		}
		return SaveMessage(IsReply);
	}
	return FALSE;
}

int SaveMessage(char IsReply)
{
	char	 *pc, allUpper;
	extern SListBase BadWords;
	extern char BadMessages[];

		/* Asshole check */
	if (idiotMessage()) {
		strCpy(msgBuf.mbtext, "Vandalism attempt.");
		aideMessage(NULL,FALSE);
		return FALSE;
	}

	if ((!loggedIn || thisRoom != MAILROOM) &&
		SearchList(&BadWords, msgBuf.mbtext) != NULL) {
		if (++IckyCount > IckyLevel && !aide) {
			logMessage(SET_FLAG, 0l, LOG_EVIL);
			if (!onConsole) HangUp(TRUE);
		}
		if (!aide) {
			if (strlen(BadMessages) != 0) {
				DiscardMessage(roomBuf.rbname, BadMessages);
			}
			return FALSE;
		}
	}

	for (pc=msgBuf.mbtext, allUpper=TRUE; *pc && allUpper;  pc++) {
		if (toUpper(*pc) != *pc && *pc != 'l')
			allUpper = FALSE;
	}
	if (allUpper)   fakeFullCase(msgBuf.mbtext);

	if (!logBuf.lbflags.TWIT)
		putMessage(&logBuf, 0);

	AnonMsgCount++;

	if (!IsReply && ++RoomMsgCount[thisRoom] > cfg.ParanoiaLimit &&
							!aide) {
		logMessage(SET_FLAG, 0l, LOG_BADWORD);
		if (!onConsole) HangUp(TRUE);
	}
	return TRUE;
}

/*
 * HasWritePrivs()
 *
 * This checks to see if the write privileges are yes.
 */
char HasWritePrivs()
{
    extern SListBase Moderators;

    if (!loggedIn && !cfg.BoolFlags.unlogEnterOk && thisRoom != MAILROOM)
	return FALSE;
    if (roomBuf.rbflags.READ_ONLY && !aide &&
	knowRoom(&logBuf, thisRoom) != WRITE_PRIVS &&
	!(strCmpU(logBuf.lbname, AskForNSMap(&Moderators, 
					thisRoom)) == SAMESTRING &&
				strlen(logBuf.lbname) != 0))
	return FALSE;
    else
	return TRUE;
}

/*
 * EatIcky()
 *
 * This function will eat an icky word for the MakeList handler.
 */
void *EatIcky(char *str)
{
    if (strlen(str) != 0) return strdup(str);
    return NULL;
}

/*
 * FindIcky()
 *
 * This will check to see if a word exists in the current message.  This is
 * used by the list handling functions.
 */
void *FindIcky(char *word, char *str)
{
    char *c;

    if ((c = matchString(str, word, lbyte(str) - 1)) != NULL)
	splitF(netLog, "FindIcky found word -%s- in message.\n", word);
    return c;
}

#define MAKE_NETTED	"Make message netted"
/*
 * moveMessage()
 *
 * This function moves a message for pullIt().
 */
static char moveMessage(char which, int m, char *toReturn)
{
    label blah;
    int   roomTarg, ourRoom;
    int   curRoom;
    char  tempauth[MB_AUTH];

    curRoom = thisRoom;

    if (!getXString("where", blah, 20, oldTarget, oldTarget))
	return FALSE;

    if ((roomTarg = roomCheck(roomExists, blah)) == ERROR) {
	if ((roomTarg = roomCheck(partialExist, blah)) == ERROR) {
	    mPrintf("'%s' does not exist.", blah);
	    return FALSE;
	}
	else {
	    thisRoom = roomTarg;
	    if (roomCheck(partialExist, blah) != ERROR) {
		thisRoom = curRoom;
		mPrintf("'%s' is not a unique string.", blah);
		return FALSE;
	    }
	    thisRoom = curRoom;
	}
    }

    strCpy(oldTarget, roomTab[roomTarg].rtname);

    ourRoom = thisRoom;
    DelMsg(which == 'M', m);
    getRoom(roomTarg);

    noteAMessage(roomBuf.msg, MSGSPERRM, pulledMId, pulledMLoc);
    putRoom(thisRoom);
    noteRoom();

    /* is message is going to a shared room ... */
    if (roomBuf.rbflags.SHARED) {
	/* if message was originally netted or if aide wants message shared */
	if (strlen(msgBuf.mboname) != 0 || getYesNo(MAKE_NETTED))
	    MakeNetted(MSGSPERRM - 1, TRUE);
    }

    getRoom(ourRoom);
    strCpy(tempauth, msgBuf.mbauth);
    ZeroMsgBuffer(&msgBuf);
    sprintf(
	msgBuf.mbtext,
	"Following message from %s %sed from %s",
	(tempauth[0]) ? tempauth : "<anonymous>",
	(which == 'M') ? "mov" : "copi",
	formRoom(thisRoom, FALSE, FALSE));
    sprintf(lbyte(msgBuf.mbtext), " to %s by %s",
	formRoom(roomTarg, FALSE, FALSE),
	logBuf.lbname
    );
    aideMessage(NULL, /* noteDeletedMessage == */ TRUE);
    *toReturn = (which == 'M') ? DELETED : NO_CHANGE;
    return TRUE;
}

/*
 * mPeek()
 *
 * This function dumps a sector in msgBuf.  sysop debugging tool.
 */
void mPeek()
{
#ifdef TEST_SYS
    char visible();
    char blup[50];
    DATA_BLOCK peekBuf;
    int  col, row;
    MSG_NUMBER r, s;

    sprintf(blup, " sector to dump (between 0 - %d): ", cfg.maxMSector);
    s = getNumber(blup, 0l, (MSG_NUMBER) (cfg.maxMSector-1));
    r = s * MSG_SECT_SIZE;
    fseek(msgfl, r, 0);
    fread(peekBuf, MSG_SECT_SIZE, 1, msgfl);
    crypte(peekBuf, MSG_SECT_SIZE, 0);
    for (row = 0;  row < 2;  row++) {
	mPrintf("\n ");
	for (col = 0;  col < 64;  col++) {
	    mPrintf("%c", visible(peekBuf[row*64 +col]));
	    if (!isprint(peekBuf[row*64 +col]))
		mPrintf("(%x)", peekBuf[row*64 +col]);
	}
    }
#else
    printf("Disabled\n");
#endif
}

#ifdef TEST_SYS
void MsgShow()
{
	MSG_NUMBER s;
	int        rover;

	s = getNumber("Message #", 0l, (MSG_NUMBER) (1000000));
	for (rover = 0; rover < MSGSPERRM; rover++) {
		if (s == roomBuf.msg[rover].rbmsgNo) {
			break;
		}
	}
	if (rover >= MSGSPERRM) {
		mPrintf("Couldn't find %ld.\n ", s);
	}
	else if (!findMessage(roomBuf.msg[rover].rbmsgLoc,
	                      roomBuf.msg[rover].rbmsgNo, TRUE)) {
		mPrintf("Message %ld not available?\n ", s);
	}
	else {
		mPrintf("\n %ld (%s):\n ", s, msgBuf.mbId);
		mPrintf("\n Author    '%s'\n ", msgBuf.mbauth);
		mPrintf("\n Room      '%s'\n ", msgBuf.mbroom);
		mPrintf("\n Date      '%s'\n ", msgBuf.mbdate);
		mPrintf("\n OrigSys   '%s'\n ", msgBuf.mboname);
		mPrintf("\n OrigSysId '%s'\n ", msgBuf.mborig);
	}
}
#endif

/*
 * msgToDisk()
 *
 * This puts a message to the given disk file.
 */
void msgToDisk(char *filename, char all, MSG_NUMBER id, SECTOR_ID loc,
								UNS_16 size)
{
    char *fn;
    long x;

    fn = GetDynamic(strlen(filename) + 5);
    strCpy(fn, filename);
    id &= S_MSG_MASK;
    outFlag = OUTOK;
    if (redirect(filename, APPEND_TO)) {
	if (!all) {
	    if (findMessage(loc, id, TRUE)) {
		if (size != 0) {
		    totalBytes(&x, upfd);
		    if (x > size * 1024) {
			undirect();
			FindNextFile(fn);
			rename(filename, fn);
			redirect(filename, APPEND_TO);
		    }
		}
		printMessage(0, 0);
	    }
	    else splitF(netLog, "bad findmessage!\n"    );
	}
	else {
	    showMessages(OLDaNDnEW, 0l, printMessage);
	}
	undirect();
    }
    free(fn);
}

SListBase Errors = { NULL, NULL, NULL, free, NULL };
/*
 * noteMessage()
 *
 * This function slots message into current room, delivers mail if necessary,
 * handles net mail, Who Else stuff, mail forwarding, checkpointing, room
 * archiving, etc etc etc etc etc.
 */
static void noteMessage(logBuffer *lBuf, UNS_16 flags)
{
    int logRover, size = 0, newflags;
    char *fn, *realfn;
    CheckPoint Cpt;
    FILE *fd;
    extern SListBase Arch_base;
    void AssembleMessage();

    KillList(&FwdVortex);
    ArchiveMail = FALSE;
    ++cfg.newest;

    if (thisRoom != MAILROOM) {
	noteAMessage(roomBuf.msg, MSGSPERRM, cfg.newest, cfg.catSector);
	/* write it to disk:	    */
	putRoom(thisRoom);
	noteRoom();
    } else {		/* when in Mail... */
	/*
	 * First, we handle the origin of this message in Mail>.  Note that
	 * checking loggedIn handles both anonymous and incoming NET mail --
	 * loggedIn is always false when in net mode, or should be!
	 */
	if (loggedIn && !(flags & SKIP_AUTHOR)) {
	    noteAMessage(lBuf->lbMail, MAILSLOTS, cfg.newest, cfg.catSector);
	    if (lBuf == &logBuf)
		 putLog(&logBuf, thisLog);
	    ArchiveMail = (strCmpU(cfg.SysopName, logBuf.lbname) == SAMESTRING);
	}

	if (flags & FORWARD_MAIL) {
	    newflags = FORCE_ROUTE_MAIL;
	    RunListA(&TempForward, AddMail, &newflags);
	}

	/*
	 * If there are overrides on delivery target, process them in
	 * preference to the mbto and mbCC fields.
	 */
	if (HasOverrides(&msgBuf)) {
	    RunListA(&msgBuf.mbOverride, AddMail, NULL);
	}
	else if (!(flags & SKIP_RECIPIENT)) {
	    if (msgBuf.mbaddr[0] ||
			strCmpU(msgBuf.mbto, lBuf->lbname) != SAMESTRING) {
		/* kinda silly, but .. */

	    if (msgBuf.mbaddr[0] && inNet == NON_NET) {
		if (strCmpU(msgBuf.mbaddr, ALL_LOCALS) != SAMESTRING)
		    AddNetMail(msgBuf.mbaddr, CREDIT_SENDER);
		else {
		    for (logRover = 0; logRover < cfg.netSize; logRover++) {
			if (netTab[logRover].ntflags.in_use &&
					netTab[logRover].ntflags.local &&
				(netTab[logRover].ntMemberNets & ALL_NETS)) {
			    getNet(logRover, &netBuf);
			    AddNetMail("", CREDIT_SENDER);
			}
		    }
		}
	    }
	    else if ((logRover = PersonExists(msgBuf.mbto)) == ERROR) {
		mPrintf("Internal error in mail (-%s-)!\n ", msgBuf.mbto);
		return ;
	    }
	    else if (logRover == cfg.MAXLOGTAB) {    /* special recipient */
		if (strCmpU(msgBuf.mbto, "Citadel") == SAMESTRING) {
		    if (!msgBuf.mbaddr[0]) {    /* Not netward bound?? */
			for (logRover = 0; logRover < cfg.MAXLOGTAB; 
					logRover++)
			    if (logRover != thisLog) {
				printf("Log %d\r", logRover); /* Notify sysop */
				getLog(&logTmp, logRover);

				if (logTmp.lbflags.L_INUSE) {
				    noteAMessage(logTmp.lbMail, MAILSLOTS,
					cfg.newest, cfg.catSector);
				    putLog(&logTmp, logRover);
				}
			    }
		    }
		}
		else {
		    AddMail(msgBuf.mbto, NULL);
		}
	    }
	    else {
		AddMail(msgBuf.mbto, NULL);
	    }
	}

	    if (inNet == NON_NET) {
		RunListA(&msgBuf.mbCC, AddMail, NULL);
	    }
	}

	if (lBuf == &logBuf)
	    fillMailRoom();			 /* update room also */
    }
	/* Finally, kill this list */
    KillList(&SysList);

    /* Checkpoint stuff - this HAS to be here before catSector is changed */
    Cpt.ltnewest = cfg.newest;
    Cpt.loc	 = cfg.catSector;

    /* make message official:   */
    cfg.catSector   = mFile1.thisSector;
    cfg.catChar     = mFile1.thisChar;
    setUp(FALSE);

    if (roomBuf.rbflags.ARCHIVE == 1 ||
		(ArchiveMail && strlen(cfg.SysopArchive) != 0)) {
	if (roomBuf.rbflags.ARCHIVE == 1)
	    fn = SearchList(&Arch_base, NtoStrInit(thisRoom, "", 0, TRUE));
	else
 	    fn = cfg.SysopArchive;

	realfn = GetDynamic(strlen(fn) + 15);

	TranslateFilename(realfn, fn);

	if (fn == NULL) {
	    sprintf(msgBuf.mbtext, "Integrity problem with Archiving: %s.",
						roomBuf.rbname);
	    aideMessage(NULL,FALSE);
	}
	else {
	    if (roomBuf.rbflags.ARCHIVE == 1)
		size = GetArchSize(thisRoom);

	    msgToDisk(realfn, FALSE, Cpt.ltnewest, Cpt.loc, size);
	}
	free(realfn);
    }

    msgBuf.mbaddr[0] = 0;
    msgBuf.mbto[0]   = 0;

    /* OK, so let's write out our checkpoint */
    if (inNet == NON_NET)	/* this is strictly performance oriented */
	if ((fd = fopen(CHECKPT, WRITE_ANY)) != NULL) {
	    fwrite(&Cpt, sizeof Cpt, 1, fd);
	    fclose(fd);
	}

    if (GetFirst(&Errors) != NULL) {
	ZeroMsgBuffer(&msgBuf);
	RunList(&Errors, AssembleMessage);
	KillList(&Errors);
	CleanEnd(msgBuf.mbtext);
	aideMessage("Net Aide",FALSE);
    }
}

/*
 * AssembleMessage()
 *
 * This adds a submessage to a message. (?)
 */
void AssembleMessage(char *str)
{
    sprintf(lbyte(msgBuf.mbtext), "%s\n\n", str);
}

/*
 * AddMail()
 *
 * This function should deliver mail to the named person.
 */
static void AddMail(char *DaPerson, int *fl)
{
    label person;
    char  system[(NAMESIZE * 2) + 10];
    int   slot, flags;
    char *InternalError = "Internal error, couldn't identify '%s'\n ";

    if (fl != NULL) flags = *fl;
    else flags = 0;

    switch (SepNameSystem(DaPerson, person, system, &netBuf)) {
    case IS_SYSTEM:
    case SYSTEM_IS_US:
	if (!NetValidate(TRUE)) return;
	AddNetMail(system, flags | CREDIT_SENDER);
	break;
    case BAD_FORMAT:
	if (inNet == NON_NET) mPrintf(InternalError, DaPerson);
	break;
    case NO_SYSTEM:
	if (inNet == NON_NET) mPrintf(InternalError, system);
	break;
    case NOT_SYSTEM:
	if (strCmpU(DaPerson, "sysop") == SAMESTRING) {
	    ArchiveMail = TRUE;
	    if ((slot = findPerson(cfg.SysopName, &logTmp)) == ERROR) {
		getRoom(AIDEROOM);
		/* enter in Aide> room -- 'sysop' is special */
		noteAMessage(roomBuf.msg, MSGSPERRM,
						cfg.newest, cfg.catSector);

		/* write it to disk:	    */
		putRoom(AIDEROOM);
		noteRoom();

		getRoom(MAILROOM);
	    }
	    else
		MailWork(slot);
	}
	else if ((slot = findPerson(DaPerson, &logTmp)) == ERROR) {
	    if (inNet == NON_NET) {
		mPrintf(InternalError, DaPerson);
	    }
	    else
		splitF(netLog, "No recipient: %s\n", DaPerson);
	}
	else {
	    MailWork(slot);
	}
    }
}

/*
 * MailWork()
 *
 * This function is central to Mail delivery, and handles forwarding of both
 * sorts.
 */
static void MailWork(int slot)
{
	logBuffer lBuf;
	int auth_slot;

	if (inNet != NON_NET) splitF(netLog, "Mail for %s.\n", logTmp.lbname);

	if (inNet != NON_NET) {
		if ((auth_slot = PersonExists(msgBuf.mbauth)) != ERROR) {
			if (!AcceptableMail(auth_slot, slot)) {
				splitF(netLog, "Mail rejected.\n");
				return;
			}
		}
	}

	noteAMessage(logTmp.lbMail, MAILSLOTS, cfg.newest, cfg.catSector);

	if (strCmpU(cfg.SysopName, logTmp.lbname) == SAMESTRING)
		ArchiveMail = TRUE;

	NetForwarding(&logTmp);

	putLog(&logTmp, slot);

	/* so we can't redeliver to this account */
	AddData(&FwdVortex, strdup(logTmp.lbname), NULL, FALSE);

	/* now check for forwarding to a local address */
	initLogBuf(&lBuf);
	LocalForwarding(thisLog, FindLocalForward(logTmp.lbname), &lBuf);
	killLogBuf(&lBuf);
}

/*
 * NetForwarding()
 *
 * This handles network forwarding.
 */
static void NetForwarding(logBuffer *lBuf)
{
    int cost;
    ForwardMail *data;
    label domain;
    char *system;
    extern SListBase MailForward;

    /* Has forwarding address?  */
    if ((data = SearchList(&MailForward, lBuf->lbname)) != NULL &&
					lBuf->lbflags.NET_PRIVS) {
	system = strdup(data->System);
	if (ReqNodeName("", system, domain, RNN_ONCE | RNN_QUIET, &netTemp)) {
	    if (domain[0] == 0)
		cost = !netTemp.nbflags.local;
	    else
		cost = FindCost(domain);
	    free(system);
	    system = strdup(data->System);
	    if (lBuf->credit >= cost) {
		AddData(&msgBuf.mbInternal, strdup(lBuf->lbname), NULL, FALSE);
		AddNetMail(system, 0);
		KillList(&msgBuf.mbInternal);
	    }
	}
	else {
	    KillData(&MailForward, lBuf->lbname);
	    UpdateForwarding();
	}
	free(system);
    }
}

/*
 * LocalForwarding()
 *
 * This handles forwarding to a local account.  Since multiple forwarding
 * may* be setup (seems unlikely), this is recursive.  A list is kept of
 * recipients of mail in order to avoid both duplicate deliveries and
 * infinite forwarding vortexes.
 */
static void LocalForwarding(int from, char *name, logBuffer *workBuf)
{
	int   slot;

	/* if this is NULL then there is no more forwarding to do */
	if (name == NULL) return;

	/* see if this account has already received the mail */
	if (SearchList(&FwdVortex, name) != NULL) return;

	if ((slot = findPerson(name, workBuf)) == ERROR)
		return;		/* implies an outofdate account */

	if (!AcceptableMail(from, slot))
		return;		/* stop a cheat, silently */

	/* OK, save the message */
	noteAMessage(workBuf->lbMail, MAILSLOTS, cfg.newest, cfg.catSector);

	/* check the network forwarding for this account */
	NetForwarding(workBuf);

	putLog(workBuf, slot);
	AddData(&FwdVortex, strdup(name), NULL, FALSE);

	LocalForwarding(slot, FindLocalForward(name), workBuf);
}

/*
 * AddNetMail()
 *
 * This should manage adding mail to a net system.
 */
static void AddNetMail(char *system, int flags)
{
    int cost, slot;
    logBuffer *lBuf;
    char isdomain = FALSE, *domain, *System;

    /*
     * sometimes system is mbaddr, which is not good, because later on down
     * the line we call findMessage, which will result in (unfortunately)
     * mbaddr being overwritten.  So we dup system.
     */
    System = strdup(system);
    if (flags & CREDIT_SENDER) lBuf = &logBuf;
    else		 lBuf = &logTmp;

    if (strlen(System)) {
	isdomain = (domain = strchr(System, '_')) != NULL;
	if (!isdomain) {
	    slot = searchNameNet(System, &netTemp);
	    cost = !netTemp.nbflags.local;
	}
	else {
	    *domain++ = 0;
	    NormStr(domain);
	    NormStr(System);
	    cost = FindCost(domain);
	}
    }
    else {
	slot = thisNet;
	cost = 0;
	getNet(thisNet, &netTemp);	/* &L mail */
	system = netTemp.netName;
    }

    if (cost > lBuf->credit && !(lBuf == &logBuf && HalfSysop())) {
	free(System);
	return ;
    }

    lBuf->credit -= cost;

    if (SearchList(&SysList, system) == NULL) {
	AddData(&SysList, strdup(system), NULL, FALSE);
	netMailOut(isdomain, System, domain, TRUE, slot, flags);
    }
    free(System);

    if (lBuf->credit < 0)
	lBuf->credit = 0;
}

/*
 * noteAMessage()
 *
 * This should add a message pointer to any room.
 */
void noteAMessage(theMessages *base, int slots, MSG_NUMBER id, SECTOR_ID loc)
{
    int  i;

    /* store into current room: */
    /* slide message pointers down to make room for this one:	 */
    for (i = 0;  i < slots - 1;  i++) {
	base[i].rbmsgLoc  = base[i+1].rbmsgLoc;
	base[i].rbmsgNo   = base[i+1].rbmsgNo ;
    }

    /* slot this message in:	*/
    base[slots-1].rbmsgNo     = id ;
    base[slots-1].rbmsgLoc    = loc;
}

/*
 * printMessage()
 *
 * This prints the indicated message on modem & console.
 */
char printMessage(int status, int slot)
{
    int  moreFollows;
    int  oldTermWidth;
    int  strip;
    extern char CCfirst;

    oldTermWidth = termWidth;
    if (outPut == DISK) {
	termWidth = cfg.ArchiveWidth;
    }
    doCR();

    mPrintf("%s", formHeader(sendTime));

    doCR();

    /* Print out who is on the CC list for this message. */
    ShowCC(SCREEN);

    EOP = TRUE;

    if (status == 0) {
	if (outFlag != OUTSKIP && outFlag != OUTNEXT)
	    while (1) {
		moreFollows     = dGetWord(msgBuf.mbtext, 150);
		    /* strip control Ls out of the output		   */
		for (strip = 0; msgBuf.mbtext[strip] != 0; strip++)
		    if (msgBuf.mbtext[strip] == 0x0c ||
			msgBuf.mbtext[strip] == SPECIAL)
			 msgBuf.mbtext[strip] = ' ';
		putWord(msgBuf.mbtext, oChar, doCR);
		if (!(moreFollows  &&  !mAbort())) {
		    if (outFlag == OUTNEXT) 	 /* If <N>ext, extra line */
			doCR();
		    break;
		}
		if (outFlag == OUTSKIP) break;
	    }
    }
    else {
	mFormat(msgBuf.mbtext, oChar, doCR);
    }
    if (EndWithCR) doCR();
    termWidth = oldTermWidth;
    return TRUE;
}

/*
 * pullIt()
 *
 * This is a sysop special to remove or otherwise manipulate a message in a
 * room.
 */
char pullIt(int m)
{
	char  toReturn;
	char  answer, net_status = -1;
	char *DelOpts[] = {
		"Delete message\n", "Move message\n", "Copy message\n",
		"Abort\n", " ", ""
	};

	/* confirm that we're removing the right one:	 */
	outFlag = OUTOK;
	if (findMessage(roomBuf.msg[m].rbmsgLoc, roomBuf.msg[m].rbmsgNo, TRUE))
		printMessage(0, 0);

	if (roomBuf.rbflags.SHARED && !msgBuf.mboname[0]) {
		ExtraOption(DelOpts, "Net message");
		net_status = 0;
	}
	/* .656 */
	else if (roomBuf.rbflags.SHARED && msgBuf.mboname[0] &&
		/* can only un-net messages generated on this system */
		strcmp(msgBuf.mboname, cfg.codeBuf + cfg.nodeName) == 0) {
		net_status = 1;
		ExtraOption(DelOpts, "Normal message");
	}

	RegisterThisMenu(NULL, DelOpts);

	do {
		outFlag = IMPERVIOUS;
		PagingOff();
		TellRoute();
		mPrintf("\n <D>elete <M>ove <C>opy <A>bort");
		if (net_status == 0)
			mPrintf(" <N>et");
		else if (net_status == 1)
			mPrintf(" <N>ormal");
		mPrintf("? (D/M/C/A%s) ", (net_status != -1) ? "/N" : "");
		switch ((answer = GetMenuChar())) {
		case 'D':
			if (deleteMessage(m))
				return DELETED;
			break;
		case 'M':
		case 'C':
			if (moveMessage(answer, m, &toReturn))
				return toReturn;
			break;
		case 'A':
			return NO_CHANGE;
		case 'N':
		/*
		 * second arg controls whether the message is marked netted or
		 * non-netted
		 * .656
		 */
			return MakeNetted(m, !msgBuf.mboname[0]);
		}
	} while (onLine());

	return DELETED;
}

/*
 * putMessage()
 *
 * This function stores a message to disk.
 * Always called before noteMessage() -- newest not ++ed yet.
 * Returns: TRUE on successful save, else FALSE
 */
char putMessage(logBuffer *lBuf, UNS_16 flags)
{
    char *s;
    extern char *ALL_LOCALS, *WRITE_LOCALS;
    extern char *R_SH_MARK, *LOC_NET, *NON_LOC_NET;
    void dLine();

    startAt(msgfl, &mFile1, cfg.catSector, cfg.catChar);
				    /* tell putMsgChar where to write   */
    if (cfg.BoolFlags.mirror)
	startAt(msgfl2, &mFile2, cfg.catSector, cfg.catChar);

    putMsgChar(0xFF);		 /* start-of-message		 */

    /* write message ID */
    dPrintf("%lu", cfg.newest + 1);

    if (inNet != NON_NET ||
	(!roomBuf.rbflags.ANON || strCmp(msgBuf.mbdate, "****") != SAMESTRING)){
	/* write date:	 */
	if (msgBuf.mbdate[0]) {
	    dPrintf("D%s", msgBuf.mbdate);
	}
	else {
	    dPrintf("D%s", formDate());
	}

	/* write time:	 */
	if (msgBuf.mbtime[0]) {
	    dPrintf("C%s", msgBuf.mbtime);
	}
	else {
	    dPrintf("C%s", Current_Time());
	}

	/* write author's name out:	 */
	if (msgBuf.mbauth[0]) {
	    dPrintf("A%s", msgBuf.mbauth);
	}
    }
    else {
	dPrintf("D****");
    }

    /* write room name out:	     */
    dPrintf("R%s", msgBuf.mbroom[0] ? msgBuf.mbroom : roomBuf.rbname);

    if (msgBuf.mbto[0]) {	 /* private message -- write addressee   */
	dPrintf("T%s", msgBuf.mbto);
    }

    if (msgBuf.mboname[0]) {
	dPrintf("N%s", msgBuf.mboname);
    }

    if (msgBuf.mbdomain[0]) {
	dPrintf("X%s", msgBuf.mbdomain);
    }

    if (msgBuf.mborig[0])   {
	dPrintf("O%s", msgBuf.mborig);
    }

	/* this convolution lets us retrace routing for shared rooms */
    if (msgBuf.mbaddr[0]) {     /* net message routing */
			/* generated by user */
	if (inNet == NON_NET || (strCmp(msgBuf.mbaddr, LOC_NET) != SAMESTRING &&
		strCmp(msgBuf.mbaddr, NON_LOC_NET) != SAMESTRING))
	    dPrintf("Q%s", wrNetId(msgBuf.mbaddr));
	else	    /* saving a net message		 */
	    dPrintf("Q%s%d", wrNetId(msgBuf.mbaddr), thisNet);

	if (strCmpU(msgBuf.mbaddr, R_SH_MARK  ) == SAMESTRING ||
			 strCmpU(msgBuf.mbaddr, NON_LOC_NET) == SAMESTRING)
	    roomTab[thisRoom].rtlastNetAll = cfg.newest + 1;
	else if (strCmpU(msgBuf.mbaddr, LOC_NET  ) == SAMESTRING)
	    roomTab[thisRoom].rtlastNetBB = cfg.newest + 1;
    }

    if (msgBuf.mbsrcId[0])   {
	dPrintf("S%s", msgBuf.mbsrcId);
    }

    if (msgBuf.mbOther[0])   {
	dPrintf("P%s", msgBuf.mbOther);
    }

    if (msgBuf.mbMsgStat[0])   {
	dPrintf("H%s", msgBuf.mbMsgStat);
    }

    if (msgBuf.mbreply[0])
	dPrintf("w%s", msgBuf.mbreply);		/* back ptr */

    /* This writes out the list of CC people to the message base. */
    /* Note we don't usually write Overrides to the message base. */
    RunListA(&msgBuf.mbCC, DisplayCC, (void *) MSGBASE);

    /* save foreign fields */
    RunList(&msgBuf.mbForeign, dLine);

    /* write message text by hand because it would overrun dPrintf buffer: */
    putMsgChar('M');    /* M-for-message.	 */
    for (s = msgBuf.mbtext;  *s;  s++) putMsgChar(*s);

    putMsgChar(0);	 /* null to end text	 */
    flushMsgBuf();

    noteMessage(lBuf, flags);

    return  TRUE;
}

/*
 * dLine()
 *
 * This prints a line to the msg base, including the NULL byte.
 */
void dLine(char *garp)
{
    do
	putMsgChar(*garp);
    while (*garp++);
}

/*
 * netMailOut()
 *
 * This should put the mail pointer and number into temp file for local mail,
 * or will set up the temp file for routed mail.
 *  SOMEDAY MOVE THIS INTO NETMISC!
 */
void netMailOut(char isdomain, char *system, char *domain, char MsgBase,
							int slot, int flags)
{
	FILE	*fd;
	label	temp, id = "";
	int	 result;
	DOMAIN_FILE fn;
	extern char *APPEND_ANY, *WRITE_ANY;
	struct	netMLstruct buf;

	if (isdomain) {
		if ((result = DomainMailFileName(fn, domain, id, system))
							==LOCALROUTE) {
			isdomain = FALSE;
			slot = searchNameNet(system, &netTemp);
			if (slot == -1) {
				slot = searchNameNet(UseNetAlias(system,FALSE), &netTemp);
			}
		}
	}
	else {
		result = LOCALROUTE;
		if (!MsgBase) slot = searchNameNet(system, &netTemp);
		if (slot == ERROR) {
			splitF(netLog, "BUG!  Slot is -1 for %s.\n", system);
			return;
		}
	}

	if (!(flags & FORCE_ROUTE_MAIL) && !isdomain && MsgBase &&
						inNet == NON_NET) {
		sprintf(temp, "%d.ml", slot);
		makeSysName(fn, temp, &cfg.netArea);
		if ((fd = fopen(fn, APPEND_ANY)) == NULL) {
			crashout("putMessage -- couldn't open direct mail file!");
		}
		buf.ML_id  = cfg.newest;
		buf.ML_loc = cfg.catSector;
		putMLNet(fd, buf);
		fclose(fd);
		netTemp.nbflags.normal_mail = TRUE;
		putNet(slot, &netTemp);
	}
	else {
		MakeIntoRouteMail(result, fn, isdomain, system, domain, MsgBase, slot);
	}
}

/*
 * MakeIntoRouteMail()
 *
 * This will set up a Rx.x file.
 */
void MakeIntoRouteMail(int result, DOMAIN_FILE fn, char isdomain, char *system,
			char *domain, char OriginIsMsgBase, int slot)
{
    int		index;
    label	temp;
    label	name;
    char	For[(2 * NAMESIZE) + 10];
    extern char PrTransmit;

    if (result == LOCALROUTE) {
	strCpy(name, UseNetAlias(netTemp.netName, TRUE));
	index = FindRouteIndex(slot);
	sprintf(temp, "R%d.%d", slot, index);
	makeSysName(fn, temp, &cfg.netArea);
	strCpy(For, netTemp.netName);
    }
    else sprintf(For, "%s _ %s", system, domain);
    if ((upfd = fopen(fn, WRITE_ANY)) == NULL) {
	printf("filename is -%s-\n", fn);
	crashout("couldn't open route mail file!");
    }
    NetPrintf(putFLChar, "%-20s", (isdomain) ? " " : netTemp.netId);
    NetPrintf(putFLChar, "%-20s", (isdomain) ? system : name);

    if (OriginIsMsgBase)
	findMessage(cfg.catSector, cfg.newest, FALSE);  /* we use false here */
    StartEncode(putFLChar);
    PrTransmit = FALSE;
    prNetStyle(!OriginIsMsgBase, (OriginIsMsgBase) ? getMsgChar : getNetChar,
			Encode, OriginIsMsgBase, For);
    PrTransmit = TRUE;
    StopEncode();
    fclose(upfd);
    if (!isdomain) {
	if (inNet == NON_NET || thisNet != slot) {
	    getNet(slot, &netTemp);
	    netTemp.nbflags.HasRouted = TRUE;
	    netTemp.nbHiRouteInd = index + 1;
	    putNet(slot, &netTemp);
	}
	else {
	    netBuf.nbflags.HasRouted = TRUE;
	    netBuf.nbHiRouteInd = index + 1;	/* saved by net stuff */
	}
    }
    else DomainFileAddResult(domain, system, "", DOMAIN_SUCCESS);
}

/*
 * putMsgChar()
 *
 * This writes successive message chars to disk.
 *
 * Globals:	thisChar=	thisSector=
 * Returns:	ERROR if problems else TRUE
 */
int putMsgChar(char c)
{
    int  toReturn;
    int  count1, count2;

    toReturn = TRUE;
    count1 = doActualWrite(msgfl, &mFile1, c);
    if (cfg.BoolFlags.mirror) {
	count2 = doActualWrite(msgfl2, &mFile2, c);
	if (count1 != count2) printf("Mirror msg count discrepancy!");
    }
    if (count1)
	++cfg.oldest;	/* wrote over a message */
    return toReturn;
}

/*
 * showMessages()
 *
 * This function will try to print a roomfull of messages.
 */
int showMessages(int flags, MSG_NUMBER LastMsg, ValidateShowMsg_f_t *Style)
{
    int		i, start, finish, increment, MsgCount = 0, result;
    MSG_NUMBER	lowLim, highLim, msgNo;
    char	pulled, PEUsed = FALSE, LoopIt, xpage;

    setUp(FALSE);
    if (flags & PAGEABLE) {
	PagingOn();
    }

    /* Don't need to check net status 'cuz netMail is sent differently. */
    if (thisRoom == MAILROOM && !loggedIn) {
	printHelp("POLICY.HLP", HELP_BITCH | HELP_NO_LINKAGE);
	if (flags & PAGEABLE)
	    if (!(flags & MSG_LEAVE_PAGEABLE))
		PagingOff();
	return 1;
    }

    if (!expert && TransProtocol == ASCII && inNet == NON_NET)
	mPrintf("\n <J>ump <N>ext <P>ause <S>top");

    /* This shouldn't bother the net. */
    if (whichIO != CONSOLE && thisRoom == MAILROOM) echo = CALLER;

    SetShowLimits(flags & REV, &start, &finish, &increment);

    if (Showing == WHATEVER) Showing = MSGS;

    switch (READMSG_TYPE(flags))   {
    case NEWoNLY:
	lowLim  = LastMsg + 1l;
	highLim = cfg.newest;
	if (inNet == NON_NET && !(flags & REV) && TransProtocol == ASCII &&
	     thisRoom != MAILROOM && oldToo) {
	    for (i = MSGSPERRM - 1; i != -1; i--)
		if (lowLim > roomBuf.msg[i].rbmsgNo &&
		    roomBuf.msg[i].rbmsgNo >= cfg.oldest)
		    break;
	    if (i != -1) {
		LoopIt = TRUE;
		while (i != -1 && LoopIt) {
		    LoopIt = FALSE;
		    findMessage(roomBuf.msg[i].rbmsgLoc,
					roomBuf.msg[i].rbmsgNo, TRUE);
		    (*Style)(1, i);

		    if ((flags & PAGEABLE) && TransProtocol == ASCII)
			if (!MoreWork(TRUE)) {
			}

		    /* Pause-Enter for the last old on new feature */
		    if (MsgStreamEnter) {
			LoopIt = TRUE;
			PEUsed = TRUE;
			if (InterruptMessage()) --i;
		    }
		}
	    }
	}
	break;
    case OLDaNDnEW:
	lowLim  = cfg.oldest;
	highLim = cfg.newest;
	break;
    case OLDoNLY:
	lowLim  = cfg.oldest;
	highLim = LastMsg;
	break;
    }

lcd(61);
    /* stuff may have scrolled off system unseen, so: */
    if (cfg.oldest  > lowLim) {
	lowLim = cfg.oldest;
    }

    /*
     * We'll increment this loop at the end.  Doing so eases the job of
     * implementing the Pause-E option.
     */
    for (i = start; i != finish && (onLine() || inNet == NET_CACHE); ) {
	if (outFlag != OUTOK) {
	     if (outFlag == OUTNEXT || outFlag == OUTPARAGRAPH)
		outFlag = OUTOK;
	    else if (outFlag == OUTSKIP)   {
		echo = BOTH;
		if (flags & PAGEABLE)
		    if (!(flags & MSG_LEAVE_PAGEABLE))
			PagingOff();
		return MsgCount;
	    }
	}
lcd(62);

	/* first get the REAL msgNo -- this is a kludge, replace next m. r. */
	msgNo = (roomBuf.msg[i].rbmsgNo & S_MSG_MASK);

	/*
	 * Now check to see if msg is in "to be read" range, OR if we are
	 * reading New AND the message is marked as SKIPPED (only happens in
	 * Mail).  Note at the moment we're not going to worry about net
	 * mode -- we don't use this loop for sending Mail, although we do
	 * for other rooms.
	 */
lcd(63);
	if (
		(msgNo >= lowLim && highLim >= msgNo) ||
		(READMSG_TYPE(flags) == NEWoNLY &&
				msgNo != roomBuf.msg[i].rbmsgNo &&
		 msgNo > cfg.oldest)
	 ) {
lcd(64);

	    if (findMessage(roomBuf.msg[i].rbmsgLoc, msgNo, TRUE)) {
lcd(65);
	    	ReverseMessage = FALSE;
		if ((*Style)(0, i)) {	/* successful print? */
lcd(66);

		    MsgCount++;

			/* message paging implementation */
		    if ((flags & PAGEABLE) && TransProtocol == ASCII) {
			if (!MoreWork(TRUE)) {
			}
		    }
lcd(67);
		    /*  Pull current message from room if flag set */
		    if (pullMessage) {
			pullMessage = FALSE;
			xpage = Pageable;
			pulled = pullIt(i);
			Pageable = xpage;
			CurLine = 1;
			outFlag = OUTOK;
			switch (pulled) {
			case NO_CHANGE:
				break;
			case DELETED:
				if (flags & REV)   i++;
				break;
			case NETTED:
				if (!(flags & REV))   i--;
				break;
			}
			doCR();
		    }
		    else
			pulled = FALSE;

		    if (ReverseMessage) {
			if (flags & REV) flags &= ~REV;
			else		 flags |= REV;
			SetShowLimits(flags & REV, &start, &finish, &increment);
		    }

lcd(68);
		    if (journalMessage) {
			msgToDisk("", FALSE, msgNo, roomBuf.msg[i].rbmsgLoc,
									0);
			journalMessage = FALSE;
		    }

		    /* Pause-E option */
		    if (MsgStreamEnter) {
			if (thisRoom == MAILROOM) {
			    ShowReply(i);
			    MsgStreamEnter = FALSE;
			    Showing = MSGS;
			    outFlag = OUTOK;	/* so we can Pause later */
			}
			else {
			    PEUsed = TRUE;
			    if (InterruptMessage()) i--;
			}
			if (flags & PAGEABLE) {
			    PagingOn();
			}
			continue;	/* skip the increment - reprint msg */
		    }

lcd(69);
		    if (
			Showing == MSGS
			&&
			outFlag != OUTSKIP	/* so we can <S>top Mail */
			&&
			!pulled
			&&
			thisRoom  == MAILROOM
			&&
			READMSG_TYPE(flags) == NEWoNLY
			&&
			canRespond()
			&&
			(strCmpU(msgBuf.mbauth, logBuf.lbname) != SAMESTRING
			||
			msgBuf.mborig[0] != 0)  /* i.e. is not local mail */
			&&
			strCmpU(msgBuf.mbauth, "Citadel") != SAMESTRING
			&&
			msgBuf.mbauth[0] != 0   /* not anonymous mail> */
		    ) {
			if ((result = DoRespond(roomBuf.msg[i].rbmsgLoc, msgNo))
								!= ERROR) {
			    roomBuf.msg[i].rbmsgNo &= S_MSG_MASK;
			    logBuf.lbMail[i].rbmsgNo &= S_MSG_MASK;
			    if (result == TRUE) {
				if (replyMessage(msgNo,roomBuf.msg[i].rbmsgLoc))
			    	    i--;
				if (whichIO != CONSOLE && thisRoom == MAILROOM)
			    	    echo = CALLER;   /* Restore privacy. */
				outFlag = OUTOK;
			    }
			    if (flags & PAGEABLE) {
				PagingOn();
			    }
			}
			else {
			    roomBuf.msg[i].rbmsgNo   |= (~S_MSG_MASK);
			    logBuf.lbMail[i].rbmsgNo |= (~S_MSG_MASK);
			}
		    }
		    else if (thisRoom == MAILROOM && (logBuf.lbMail[i].rbmsgNo & (~S_MSG_MASK))) {
			logBuf.lbMail[i].rbmsgNo &= S_MSG_MASK;
		    }
lcd(70);
		}
lcd(661);
	    }
	}
	i += increment;
    }

    echo = BOTH;
    Showing = WHATEVER;

    if (heldMess && PEUsed) {
	if ((flags & PAGEABLE) && logBuf.lbpage != 0&& TransProtocol == ASCII) {
	    AndMoreWork(1);
	}
	givePrompt();
	mPrintf("Current Held Message\n ");
	hldMessage(FALSE);
    }

    if (flags & PAGEABLE)
	if (!(flags & MSG_LEAVE_PAGEABLE))
	    PagingOff();
    return MsgCount;
}

/*
 * InterruptMessage()
 *
 * This handles Pause-Enter.
 */
static char InterruptMessage()
{
    char toReturn = FALSE;

    Showing = WHATEVER;
    if (heldMess) {
	if (hldMessage(FALSE)) toReturn = TRUE;
    }
    else {
	if (makeMessage(ASCII)) toReturn = TRUE;
    }

    MsgStreamEnter = FALSE;
    Showing = MSGS;
    outFlag = OUTOK;	/* so we can Pause later */

    return toReturn;
}

/*
 * ShowReply()
 *
 * This is the backlink tracer of a mail message.
 */
void ShowReply(int i)	/* i is index into roomBuf.msgs */
{
	char *ptr, doit = FALSE;
	MSG_NUMBER msg;
	char author[MB_AUTH], rec[MB_AUTH];

	/* format of mbreply is loc:msgNo */

	MsgStreamEnter = FALSE;
	outFlag = OUTOK;
	/* make sure there's a return ptr */
	if (strlen(msgBuf.mbreply) != 0 &&
		(ptr = strchr(msgBuf.mbreply, ':')) != NULL) {
		msg = atol(ptr + 1);
		doit = findMessage(atoi(msgBuf.mbreply), msg, TRUE);
		for (; i >= 0; i--)
			if (msg == (roomBuf.msg[i].rbmsgNo & S_MSG_MASK)) break;
	}
	else {	/* else do a manual scan */
		strCpy(author, msgBuf.mbauth);
		strCpy(rec, msgBuf.mbto);
		for (--i; i >= 0; i--) {
			doit = findMessage(roomBuf.msg[i].rbmsgLoc,
			roomBuf.msg[i].rbmsgNo & S_MSG_MASK, TRUE);
			if (!doit) i = 0;
			else if (strCmpU(author, msgBuf.mbto) == SAMESTRING &&
				 strCmpU(rec, msgBuf.mbauth) == SAMESTRING) break;
		}
	}

	/* this will allow streaming along on the message chain */
	if (doit) {
		printMessage(0, 0);
		if (MsgStreamEnter)
			ShowReply(i);	/* fix this argument someday */
	}
}

/*
 * SetShowLimits()
 *
 * Sets up the limits for showing messages.
 */
void SetShowLimits(char rev, int *start, int *finish, int *increment)
{
    /* Allow for reverse retrieval: */
    if (!rev) {
	*start	= 0;
	*finish	= (thisRoom == MAILROOM) ? MAILSLOTS : MSGSPERRM;
	*increment   = 1;
    } else {
	*start	= (((thisRoom == MAILROOM) ? MAILSLOTS : MSGSPERRM) -1);
	*finish	= -1;
	*increment   = -1;
    }
}

/*
 * redirect()
 *
 * This function causes output to be redirected to a file.
 */
char redirect(char *name, int flags)
{
    extern char *strFile;
    char *mode;
    char fullFileName[100];

    if (flags & APPEND_TO)
	mode = APPEND_TEXT;
    if (flags & INPLACE_OF)
	mode = WRITE_TEXT;

    if (name != NULL) strCpy(fullFileName, name);
    else fullFileName[0] = 0;

    if (strlen(fullFileName) != 0 || getXString(strFile, fullFileName, 100,
	  (strlen(jrnlFile) == 0) ? NULL : jrnlFile, jrnlFile)) {
	if ((upfd = fopen(fullFileName, mode)) == NULL) {
	    if (inNet == NON_NET)
		mPrintf("ERROR: Couldn't open output file %s\n ", fullFileName);
	}
	else {
	    outPut = DISK;
	    if (name == NULL) strCpy(jrnlFile, fullFileName);
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * undirect()
 *
 * This makes output go back to normal.
 */
void undirect()
{
    fclose(upfd);
    outPut = NORMAL;
}

#define UnknownRoute "\n Couldn't identify route (%s).", msgBuf.mbaddr
/*
 * TellRoute()
 *
 * This will figure out where this message came from.
 */
void TellRoute()
{
    extern char *LOC_NET, *NON_LOC_NET;
    int slot;

    if (strCmp(msgBuf.mboname, cfg.codeBuf + cfg.nodeName) != SAMESTRING &&
					strlen(msgBuf.mbaddr) != 0) {
	if (((slot = RoutePath(NON_LOC_NET, msgBuf.mbaddr)) != ERROR ||
	    (slot = RoutePath(LOC_NET, msgBuf.mbaddr)) != ERROR) &&
		slot >= 0 && slot < cfg.netSize) {
	    if (slot != thisNet)
	    	getNet(slot, &netBuf);

	    if (netBuf.nbflags.in_use)
		mPrintf("\n Routed from %s.", netBuf.netName);
	    else
		mPrintf(UnknownRoute);
	}
	else
	    mPrintf(UnknownRoute);
    }
}

/*
 * FindNextFile()
 *
 * This finds next route filename in sequence.
 */
int FindNextFile(char *base)
{
    int rover = 0;
    char *fn;

    fn = GetDynamic(strlen(base) + 5);
    /* Find next unused file name */
    do {
	sprintf(fn, "%s.%d", base, rover++);
    } while (access(fn, 0) != -1);
    strCpy(base, fn);
    free(fn);
    return rover - 1;
}

/*
 * TranslateFilename()
 *
 * This does translations on a filename.  This is used for embedding dates
 * or numbers into a filename.
 */
void TranslateFilename(char *realfn, char *fn)
{
    int year, day, hours, minutes;
    char *month;

    getCdate(&year, &month, &day, &hours, &minutes);
    do {
	*realfn = *fn;
	if (*fn == '%') {
	    fn++;
	    switch (*fn) {
	    case 'm':
	    case 'M':
		sprintf(realfn, "%s", month);
		break;
	    case 'y':
	    case 'Y':
		sprintf(realfn, "%d", year);
		break;
	    default:
		sprintf(realfn, "%c", *fn);
		break;
	    }
	    while (*(realfn + 1))
		realfn++;
	}
	realfn++;
    } while (*fn++);
}
