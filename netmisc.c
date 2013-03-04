/*
 *				netmisc.c
 *
 * Networking functions of miscellaneous type.
 */

/*
 *				history
 *
 * 91Aug17 HAW  New comment style.
 * 86Aug20 HAW  History not maintained due to space problems.
 */
#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"

/*
 *				contents
 *
 */

/* #define LUNATIC */
/* #define NET_DEBUG 1  */
/*
 *		External variable declarations in NET.C
 */
char *DupDomain = "Sorry, %s is listed in more than one domain.  Please use a full specification\n ";
char		ErrBuf[100];		/* General buffer for error messages */

int		AnyIndex = 0;  /* tracks who to call between net sessions */

FILE		*netLog, *netMisc, *netMsg;
static char     UsedNetMsg;
char		*nMsgTemplate = "netMsg.$$$";
char		logNetResults = FALSE;
char		inNet = NON_NET;
AN_UNSIGNED     RecBuf[SECTSIZE + 5];
int		callSlot;
label		normed, callerName, callerId;
logBuffer       *lBuf;
int		PriorityMail = 0;

static void *FindCalledNode();
static void FreeSystemCallRecord();
SListBase SystemsCalled = { NULL, FindCalledNode, NULL, FreeSystemCallRecord,
								NULL };
extern MenuId	GetListId;

static char	*SupportedBauds[] = {
	"300", "3/12", "3/24", "3/48", "3/96", "3/14.4", "3/19.2"
};

/*
 * UntilNetSessions
 *
 * This is used to maintain a list of Until-Done net sessions as requested by
 * the sysop.  Each element contains information concerning which member net
 * is involved (only one can be specified) and how long the session should
 * last.
 */
void freeUNS();
SListBase UntilNetSessions = { NULL, ChkTwoNumbers, NULL, freeUNS, NULL };

/*
 * Shutup
 * 
 * Systems to ignore if they're "new" so as not to report them.
 */
SListBase Shutup = { NULL, ChkCC, NULL, NULL, EatIcky };

/*
 *		External variable definitions for NET.C
 */
extern CONFIG    cfg;		/* Lots an lots of variables    */
extern NetBuffer netTemp;
extern logBuffer logBuf;	/* Person buffer		*/
extern logBuffer logTmp;	/* Person buffer		*/
extern aRoom     roomBuf;	/* Room buffer			*/
extern rTable    *roomTab;
extern MessageBuffer   msgBuf;
extern MessageBuffer   tempMess;
extern NetBuffer netBuf;
extern NetTable  *netTab;
extern int       thisNet;
extern char      onConsole;
extern char      loggedIn;	/* Is we logged in?		*/
extern char      outFlag;	/* Output flag			*/
extern char      haveCarrier;	/* Do we still got carrier?     */
extern char      modStat;	/* Needed so we don't die       */
extern char      WCError;
extern int       thisRoom;
extern int       thisLog;
extern char      *confirm;
extern char      heldMess;
extern int	 CurLine;
extern char      netDebug;
extern char      *AssignAddress;
extern int	 outPut;
extern FILE	 *upfd;
extern char	 *APPEND_TEXT;
extern char      remoteSysop;
extern char	 *DomainFlags;
extern char	 *ALL_LOCALS;
extern char	 *R_SH_MARK;

/*
 * called_stabilize()
 *
 * This function attempts to stabilize communication on the receiver end.
 */
char called_stabilize()
{
    char retVal = TRUE;

    retVal = getNetBaud();	/* has to handle stroll, too. */

    if (!gotCarrier()) {
	killConnection("caller_stab");
	retVal = FALSE;
    }

    return retVal;
}

static int table[2][3] = {
	{ 7, 13, 69 },
	/* { 68, 79, 35 } */
};
/*
 * check_for_init()
 *
 * This function looks for the networking initialization sequence.
 */
char check_for_init(char mode)
{
    int index;
    int	count, timeOut;
    AN_UNSIGNED thisVal, lastVal;

    index = (inNet == STROLL_CALL) ? 1 : 0;
    lastVal = (mode) ? table[index][0] : 0;
    timeOut = (INTERVALS / 2) * (25);
    for (count = 0; count < timeOut; count++) {
	if (MIReady()) {
	    thisVal = Citinp();
	    if (cfg.BoolFlags.debug) splitF(netLog, "%d ", thisVal);
	    if (thisVal == table[index][0])
		lastVal = table[index][0];
	    else if (thisVal == table[index][1]) {
		if (lastVal == table[index][0]) lastVal = table[index][1];
		else	lastVal = 0;
	    }
	    else if (thisVal == table[index][2]) {
		if (lastVal == table[index][1]) {
		    lastVal = AckStabilize(index);
		    if (lastVal == ACK) return TRUE;
		    else if (lastVal == table[index][2])
			return (AckStabilize(index) == ACK);
		    else if (lastVal != table[index][0] &&
					lastVal != table[index][1])
			return FALSE;
		}
	    }
	}
	else pause(1);
    }
    return FALSE;
}

/*
 * AckStabilize()
 *
 * This function tries to stabilize with net caller.
 */
int AckStabilize(int index)
{
    outMod(~(table[index][0]));
    outMod(~(table[index][1]));
    outMod(~(table[index][2]));
    return receive(1);
}

/* #define SENDER_DEBUG */
/*
 * AddNetMsgs()
 *
 * This function integrates messages into the data base.  Options include
 * adding the net area or not to the filename and specifying the processing
 * function rather than being stuck with a standard processing function.
 * Usually the processing function will integrate messages into the message
 * data base, although it may do negative mail checking instead.
 */
int AddNetMsgs(char *base, void (*procFn)(void), char zap, int roomNo,
								char AddNetArea)
{
#ifdef SENDER_DEBUG
	SYS_FILE work, w;
	int xcount;
#endif
	char tempNm[80];
	int count = 0, locroom;
	extern char *READ_ANY;

	if (AddNetArea)
		makeSysName(tempNm, base, &cfg.netArea);
	else
		strcpy(tempNm, base);

	if ((netMisc = fopen(tempNm, READ_ANY)) == NULL) {
		return ERROR;
	}
	if (roomNo != -1) getRoom(roomNo);
	/* If reading for mail room, prepare a log buffer. */
	if (roomNo == MAILROOM || roomNo == -1)
		lBuf = &logTmp;
	else
		lBuf = NULL;

	while (getMessage(getNetChar, TRUE, TRUE, TRUE)) {
		count++;
		if (roomNo == -1) {
			if ((locroom = roomExists(msgBuf.mbroom)) >= 0) {
				getRoom(locroom);
			}
			else continue;
		}
		if (strCmpU(cfg.nodeId+cfg.codeBuf,msgBuf.mborig)!=SAMESTRING){
			(*procFn)();
		}
	}
	fclose(netMisc);

	if (zap == 1) unlink(tempNm);
	else if (zap == 2) {
		if (count != 0) {
#ifdef SENDER_DEBUG
			if (netDebug) {
splitF(netLog, "netDebug ON\n");
				xcount = 0;
				do {
					sprintf(w, "a.%d", xcount++);
					makeSysName(work, w, &cfg.netArea);
splitF(netLog, "Checking existence of %s\n", work);
				} while (access(work, 0) != -1);
				if (rename(tempNm, work)!=0)
					splitF(netLog, "Rename(%s, %s) failed\n", tempNm, work);
else
splitF(netLog, "Rename(%s, %s) worked\n", tempNm, work);
			}
			else
#endif
			unlink(tempNm);
		}
	}
	return count;
}

/*
 * getNetChar()
 *
 * This function gets a character from a network temporary file.  The file
 * should have been opened elsewhere.
 */
int getNetChar()
{
     int c;

     c = fgetc(netMisc);
     if (c == EOF) return -1;
     return c;
}

/*
 * inMail()
 *
 * This function integrates a message into the message database.  It includes
 * recognizing bangmail, vortex activation, and bad word scanning.
 */
void inMail()
{
    extern SListBase BadWords;
    extern char BadMessages[];

    if (NotVortex()) {
	if (cfg.BoolFlags.NetScanBad) {
	    if (thisRoom != MAILROOM &&
			SearchList(&BadWords, msgBuf.mbtext) != NULL) {
		if (strlen(BadMessages) != 0)
		    DiscardMessage(roomBuf.rbname, BadMessages);
		sprintf(msgBuf.mbtext,
		 	"Decency: Net message from %s @%s in %s discarded.",
			msgBuf.mbauth, msgBuf.mboname,
			(roomExists(msgBuf.mbroom)) ?
			formRoom(roomExists(msgBuf.mbroom), FALSE, FALSE) :
			msgBuf.mbroom);
		netResult(msgBuf.mbtext);
		return;
	    }
	}
	if (AssignAddress != NULL)
	    strcpy(msgBuf.mbaddr, AssignAddress);
	putMessage(&logBuf, (inNet != NON_NET) ? SKIP_AUTHOR : 0);
    }
    else {
	DiscardMessage("", "discard");
    }
}

static int GoodCount, BadCount;
/*
 * inRouteMail()
 *
 * This function handles incoming route mail.
 */
void inRouteMail()
{
    label oname, domain;

    if (RecipientAvail()) {
	inMail();
    }
    if (BadCount) {
	sprintf(lbyte(tempMess.mbtext), " on %s _ %s was undeliverable.",
			cfg.nodeName + cfg.codeBuf,
			cfg.codeBuf + cfg.nodeDomain);
	strcpy(tempMess.mbto, msgBuf.mbauth);
	strcpy(oname, msgBuf.mboname);
	strcpy(domain, msgBuf.mbdomain);
	strcpy(tempMess.mbauth, "Citadel");
	strcpy(tempMess.mbroom, "Mail");
	strcpy(tempMess.mbtime, Current_Time());
	strcpy(tempMess.mbdate, formDate());
	sprintf(tempMess.mbId, "%lu", cfg.newest++ + 1);
	ZeroMsgBuffer(&msgBuf);
	MoveMsgBuffer(&msgBuf, &tempMess);
	netMailOut(TRUE, UseNetAlias(oname, FALSE), domain, FALSE, -1, 0);
    }
}

/*
 * RecipientAvail()
 *
 * This function checks to see if recipient is here.  This includes override
 * handling.
 */
char RecipientAvail()
{
    void RecAvWork();

    GoodCount = BadCount = 0;

    if (msgBuf.mbdomain[0]) {
	if (!HasOverrides(&msgBuf)) {
	    RecAvWork(msgBuf.mbto);
	}
	else {
	    RunList(&msgBuf.mbOverride, RecAvWork);
	}
	return GoodCount;
    }
    return TRUE;
}

/*
 * RecAvWork()
 *
 * This function does the real work of RecipientAvailable() - split out to
 * better handle other recipients.
 */
static void RecAvWork(char *name)
{
    if (PersonExists(name) == ERROR &&
		strCmpU(msgBuf.mbauth, "Citadel") != SAMESTRING) {
	BadCount++;
	splitF(netLog, "No recipient: %s\n", name);
	if (BadCount == 1)
	    sprintf(tempMess.mbtext, "Your message to %s", name);
	else
	    sprintf(lbyte(tempMess.mbtext), ", %s", name);
    }
    else if (PersonExists(name) != ERROR ||
		strCmpU(msgBuf.mbauth, "Citadel") != SAMESTRING)
    	GoodCount++;
}

/*
 * DiscardMessage()
 *
 * This function prints a message to a discard file.
 */
void DiscardMessage(char *name, char *filename)
{
    if (redirect(filename, APPEND_TO)) {
	if (strlen(name)) {
	    fprintf(upfd, "%s\n", name);
	    mPrintf("%s", formHeader(TRUE));
	}
	else mPrintf("%s (%s)", formHeader(TRUE), msgBuf.mbsrcId);
	doCR();
	mFormat(msgBuf.mbtext, oChar, doCR);
	doCR();
	doCR();
	undirect();
    }
}

/*
 * netController()
 *
 * This function acts as a buffer between netControllerWork and the rest of
 * the world.  It handles all important de-initializations and initializations.
 */
void netController(int NetStart, int NetLength, MULTI_NET_DATA whichNets,
						char mode, UNS_16 flags)
{
	SYS_FILE AideMsg;

	void netControllerWork(int NetStart, int NetLength,
				MULTI_NET_DATA whichNets,
				char mode, UNS_16 flags);

	if (loggedIn)	/* should only happen on mistake by sysop */
		terminate( /* hangUp == */ TRUE, TRUE);

	switch (mode) {
	case ANYTIME_NET:
	case UNTIL_NET:
		if (!AnyCallsNeeded(whichNets)) {
			return;
		}
		break;
	case ANY_CALL:
		while (MIReady()) Citinp();
		break;
	}

	if (logNetResults) {
		makeSysName(AideMsg, "netlog.sys", &cfg.netArea);
		if ((netLog = fopen(AideMsg, APPEND_TEXT)) == NULL)
			netResult("Network Logging: Couldn't open netLog.");
		}
	else
		netLog = NULL;

	netControllerWork(NetStart, NetLength, whichNets, mode, flags);
	inNet = NON_NET;
	KillList(&SystemsCalled);
	if (logNetResults) {
		fclose(netLog);
		netLog = NULL;
	}
}

/*
 * netControllerWork()
 *
 * This is the main manager of a network session.  It is responsible for
 * scheduling calls, noticing incoming calls, exiting network sessions due
 * to timeouts or other events, forming error reports for the Aide> room,
 * etc.
 */
static void netControllerWork(int NetStart, int NetLength,
			MULTI_NET_DATA whichNets, char mode, UNS_16 flags)
{
	SystemCallRecord *called;
	SYS_FILE AideMsg;
	int x;
	int searcher = 0, start, first;
	long waitTime, InterCallDelay;
	extern char *WRITE_TEXT, *READ_TEXT, *APPEND_TEXT;

	outFlag = OUTOK;		/* for discarding messages correctly */

	inNet = mode;

	searcher = AnyIndex;    /* so we don't always start at front */

	InterCallDelay = (!(flags & LEISURELY)) ? 2l : 15l;

	loggedIn = FALSE;			/* Let's be VERY sure.	*/
	thisLog = -1;

	splitF(netLog, "\nNetwork Session");
	splitF(netLog, "\n%s @ %s\n", formDate(), Current_Time());
	SpecialMessage("Network Session");
	logMessage(INTO_NET, 0l, 0);
	modStat = haveCarrier = FALSE;
	setTime(NetStart, NetLength);
	makeSysName(AideMsg, nMsgTemplate, &cfg.netArea);
	if ((netMsg = fopen(AideMsg, WRITE_TEXT)) == NULL)
		splitF(netLog,
		       "WARNING: Can't open %s, errno %d!!!!\n",
		       AideMsg,
		       errno);

	UsedNetMsg = FALSE;

	x = timeLeft();

	do {	/* force at least one time through loop */
		waitTime = (cfg.catChar % 5) + 1;
		while (waitTime > minimum(5, ((x/2)))) waitTime /= 2;

		if (flags & LEISURELY)
			for (startTimer(WORK_TIMER);
				chkTimeSince(WORK_TIMER) < (waitTime * 60) && 
							!KBReady();) {
				if (gotCarrier()) break;
				else BeNice(NET_PAUSE);
			}

		/* This will break us out of a network session if ESC is hit */
		if (KBReady()) 
			if (getCh() == SPECIAL) break;

		/*
		 * In case someone calls while we're doing after-call
		 * processing.
		 */
		while (gotCarrier()) { 
			modStat = haveCarrier = TRUE;
			system_called();
		}

		/* ok, make calls */
		if (cfg.netSize != 0) {
			start = searcher;
			do {
				if (needToCall(searcher, whichNets)) {
#ifdef LUNATIC
					ExplainNeed(searcher, whichNets);
#endif
					if (!HasPriorityMail(searcher))
						CacheSystem(searcher, FALSE);

					if ((called=callOut(searcher))!=NULL) {
						if (!caller())
							called->Unstable++;
						splitF(netLog, "(%s)\n",
								Current_Time());
					}
					for (startTimer(WORK_TIMER);
							!gotCarrier() &&
							chkTimeSince(WORK_TIMER)
							< InterCallDelay;)
						;
					while (gotCarrier()) {
						modStat = haveCarrier = TRUE;
						system_called();
					}
					if (whichNets == PRIORITY_MAIL) {
						getNet(thisNet, &netBuf);
						netBuf.MemberNets &= ~(PRIORITY_MAIL);
						putNet(thisNet, &netBuf);
					}
				}
				searcher = (searcher + 1) % cfg.netSize;
				if (mode == ANYTIME_NET && timeLeft() < 0 &&
						whichNets != PRIORITY_MAIL)
					break;      /* maintain discipline */
			} while (!KBReady() && searcher != start);
		}
		if (mode == ANYTIME_NET || mode == UNTIL_NET) {
			if (!AnyCallsNeeded(whichNets)) break;
		}
	} while ((x = timeLeft()) > 0);

	splitF(netLog, "\nOut of Networking Mode (%s)\n\n", Current_Time());

	for (x = 0; x < cfg.netSize; x++)
		if (netTab[x].ntMemberNets & PRIORITY_MAIL) {
			getNet(x, &netBuf);
			netBuf.MemberNets &= ~(PRIORITY_MAIL);
			putNet(x, &netBuf);
		}

	if (flags & REPORT_FAILURE) {
		if (AnyCallsNeeded(whichNets)) {
			sprintf(msgBuf.mbtext, 
			"The following systems could not be reached: ");
			for (searcher = 0, first = 1; searcher < cfg.netSize; searcher++)
				if (needToCall(searcher, whichNets)) {
					if (!first) strcat(msgBuf.mbtext,", ");
					first = FALSE;
					getNet(searcher, &netBuf);
					strcat(msgBuf.mbtext, netBuf.netName);
				}
			strcat(msgBuf.mbtext, ".");
			netResult(msgBuf.mbtext);
		}
	}

	if (inNet == ANYTIME_NET) {
		AnyIndex = searcher;    /* so we can start from here later */
	}

	fclose(netMsg);
	netMsg = NULL;

	/* Make the error and status messages generated into an Aide> msg */
	makeSysName(AideMsg, nMsgTemplate, &cfg.netArea);
	if (UsedNetMsg) {
		ZeroMsgBuffer(&msgBuf);
		if (access(AideMsg, 4) == -1) {
			sprintf(msgBuf.mbtext, "Where did '%s' go???", AideMsg);
			aideMessage("Net Aide", FALSE);
		}
		else {
			ingestFile(AideMsg, msgBuf.mbtext);
			aideMessage("Net Aide", FALSE);
		}
	}
	unlink(AideMsg);

	modStat = haveCarrier = FALSE;
	ITL_DeInit();
	logMessage(OUTOF_NET, 0l, 0);
	startTimer(NEXT_ANYNET);      /* anytime net timer */
	getRoom(LOBBY);
}

static int RunUntil;

/*
 * setTime()
 *
 * This function sets up some global variables for the networker.
 */
void setTime(int NetStart, int NetLength)
{
    int yr, hr, mins, dy, temp;
    char *mn;

    startTimer(NET_SESSION);
    if (NetLength == 0)
	RunUntil = 0;
    else {
	getCdate(&yr, &mn, &dy, &hr, &mins);
	temp = (hr * 60) + mins;
	RunUntil = 60 * (NetLength - abs(temp - NetStart));
    }
}

/*
 * timeLeft()
 *
 * This function does a rough estimate of how much time left and returns it.
 */
int timeLeft()
{
    int elapsed;

    elapsed = chkTimeSince(NET_SESSION);
    if (elapsed > RunUntil) return 0;
    return (((RunUntil - elapsed) / 60) + 1);
}

/*
 * callOut()
 *
 * This function attempts to call some other system.
 */
static SystemCallRecord *callOut(int i)
{
	SystemCallRecord *called;

	/*
	 * hopefully, the only place we add stuff to SystemsCalled
	 */
	if ((called = SearchList(&SystemsCalled, &i)) == NULL) {
		called = NewCalledRecord(i);
	}
	getNet(callSlot = i, &netBuf);
	splitF(netLog, "Calling %s @ %s (%s): ",
			netBuf.netName, netBuf.netId, Current_Time());
	strcpy(normed, netBuf.netId);		/* Cosmetics */
	strcpy(callerId, netBuf.netId);
	strcpy(callerName, netBuf.netName);
	if (makeCall(TRUE, NO_MENU)) {
		modStat = haveCarrier = TRUE;
		return called;
	}
	killConnection("callout");	/* Take modem out of call mode   */
	splitF(netLog, "No luck.\n");
	return NULL;
}

/*
 * NewCalledRecord()
 *
 * This function correctly creates, initializes, and adds to SystemsCalled a
 * new SystemCallRecord.
 */
SystemCallRecord *NewCalledRecord(int slot)
{
	SystemCallRecord *called;
	void *FindSentRoom();

	called = GetDynamic(sizeof *called);
	called->Status = SYSTEM_NOT_CALLED;
	called->Node = slot;
	called->Unstable = 0;
	InitListValues(&called->SentRooms, FindSentRoom, NULL, free, NULL);
	InitListValues(&called->SentVirtualRooms,FindSentRoom,NULL,free,NULL);
	AddData(&SystemsCalled, called, NULL, FALSE);
	return called;
}

/*
 * FindSentRoom()
 *
 * This function helps find a room record in a list of such records.  The record
 * is just a pointer to an integer.
 */
static void *FindSentRoom(int *room, int *target)
{
	if (*room == *target)
		return room;
	return NULL;
}

/*
 * FreeSystemCallRecord()
 *
 * This function frees a SystemCallRecord.  It's usually called via KillList().
 */
static void FreeSystemCallRecord(SystemCallRecord *called)
{
	KillList(&called->SentRooms);
	KillList(&called->SentVirtualRooms);
	free(called);
}

/*
 * moPuts()
 *
 * This function puts a string out to modem without carr check.
 */
void moPuts(char *s)
{
    while (*s) {
	pause(5);
	if (cfg.BoolFlags.debug) mputChar(*s);
	outMod(*s++);
    }
}

/*
 * netMessage()
 *
 * This function will send message via net.  This is a userland function.
 */
int netMessage(int uploading)
{
    if (!NetValidate(TRUE)) return FALSE;

    ZeroMsgBuffer(&msgBuf);

    if (!netInfo(TRUE)) return FALSE;
    return procMessage(uploading, FALSE);
}

/*
 * writeNet()
 *
 * This function writes nodes on the net to the screen.  Options include
 * showing only local systems and with or without their ids.
 */
void writeNet(char idsAlso, char LocalOnly)
{
    int rover, count = 0, len;

    outFlag = OUTOK;

    PagingOn();

    mPrintf("Systems on the net:\n ");
    doCR();
    for (rover = 0; outFlag != OUTSKIP && rover < cfg.netSize; rover++) {
	if (netTab[rover].ntflags.in_use &&
		(!LocalOnly || netTab[rover].ntflags.local)) {
	    getNet(rover, &netBuf);
	    if ((idsAlso || netBuf.MemberNets & ALL_NETS)) {
		/* mPrintf("%-22s", netBuf.netName); */
		mPrintf("%s", netBuf.netName);
		if (idsAlso) {
#ifdef TURBO_C_VSPRINTF_BUG
		    SpaceBug(22 - strlen(netBuf.netName));   /* EEEESH */
		    mPrintf("%-22s%-16s%-12s", netBuf.netId,
			(needToCall(rover, ALL_NETS)) ? "<need to call>" : "",
			SupportedBauds[netBuf.baudCode]);
#else
		    mPrintf("%*c%-22s%-16s%-12s", 22 - strlen(netBuf.netName),
			' ', netBuf.netId,
			(needToCall(rover, ALL_NETS)) ? "<need to call>" : "",
			SupportedBauds[netBuf.baudCode]);
#endif
		    if (netBuf.nbflags.OtherNet) mPrintf("O");
		    else if (!(netBuf.MemberNets & ALL_NETS))
			mPrintf("d");
		    if (netBuf.nbflags.MassTransfer)
			mPrintf("F");
		    doCR();
		}
		else {
		    if (strlen(netBuf.nbShort) != 0) {
			mPrintf(" (%s)", netBuf.nbShort);
			len = strlen(netBuf.nbShort) + 3;
		    }
		    else len = 0;
		    /* mPrintf(", "); */
		    if (++count % 3 == 0) {
			count = 0;
			doCR();
		    }
		    else {
#ifdef TURBO_C_VSPRINTF_BUG
			SpaceBug(28 - (len + strlen(netBuf.netName)));   /* EEEESH */
#else
			mPrintf("%*c", 28 - (len + strlen(netBuf.netName)), ' ');
#endif
		    }
		}
	    }
	}
    }
    if (!idsAlso)
	WriteDomainContents();
    PagingOff();
}

/*
 * roomsShared()
 *
 * This function returns TRUE if this system has a room with new data to share
 * (orSomething).
 */
static char roomsShared(int slot)
{
	int ROutGoing(SharedRoomData *room, int system, int roomslot, void *d);
	char OutGoing;
	SystemCallRecord *called;

	called = SearchList(&SystemsCalled, &slot);

	/*
	 * We only want to make one "successful" call per 
	 * voluntary net session
	 */
	if ((inNet == UNTIL_NET || inNet == NORMAL_NET ||
			inNet == ANYTIME_NET ) && 
			(called != NULL && called->Status == SYSTEM_CALLED))
		return FALSE;

	/*
	 * Rules:
	 * We check each slot of the shared rooms list for this node.  For
	 * each one that is in use, we do the following:
	 *		HOSTS ARE OBSOLETE!
	 * a) if we are regional host for the room and other system is
	 *    a backbone, then don't assume we need to call.
	 * b) if we are backboning the room, check to see what status of this
	 *    room for other system is.
	 *    1) If we are Passive Backbone, then we need not call.
	 *    2) If we are Active Backbone, then do call.
	 *    3) The Regional Host looks screwy.  This may be a bug.
	 * c) If none of the above applies, implies we are a simple Peon, so
	 *    we simply check to see if we have outgoing messages, and if so,
	 *    return TRUE indicating that we need to call; otherwise, continue
	 *    search.
	 *
	 * LATER NOTE: now this is split up due to the use of EachSharedRoom.
	 */
	OutGoing = FALSE;
	EachSharedRoom(slot, ROutGoing, VRNeedCall, &OutGoing);
	return OutGoing;
}

/*
 * ROutGoing()
 *
 * This decides if the system in question needs to be called due to the
 * situation of the rooms.
 */
static int ROutGoing(SharedRoomData *room, int system, int roomslot, void *d)
{
    char *arg;

    arg = d;
    if (GetMode(room->room->mode) == BACKBONE) {
	if (inNet == NORMAL_NET) {
	    *arg = TRUE;
	    return ERROR;
	}
    }
    if (
	roomTab[roomslot].rtlastNetAll > room->room->lastMess ||
	(GetMode(room->room->mode) == BACKBONE &&
		roomTab[roomslot].rtlastNetBB > room->room->lastMess)
	) {
	*arg = TRUE;
	return ERROR;
    }
    if (GetFA(room->room->mode)) {
	*arg = TRUE;
	return ERROR;
    }
    return TRUE;
}

/*
 * DumpRoom()
 *
 * This dumps out information concerning a shared room, such as the status
 * and the message stuff.
 */
int DumpRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
    char cmd, *s1, *s2, *s3, *name, doit;

    mPrintf("%s ", roomTab[roomslot].rtname);
    Addressing(system, room->room, &cmd, &s1, &s2, &s3, &name, &doit);
    mPrintf(name);

mPrintf(" (last sent=%ld, netlast=%ld, nb=%ld)",
room->room->lastMess, roomTab[roomslot].rtlastNetAll,
roomTab[roomslot].rtlastNetBB);

    mPrintf("\n ");

    return TRUE;
}

/*
 * netResult()
 *
 * This will put a message to the net msg holder, building a message for the
 * Aide room.
 */
void netResult(char *msg)
{
	if (netMsg != NULL && SearchList(&Shutup, callerName) == NULL) {
		fprintf(netMsg, "(%s) %s\n\n", Current_Time(), msg);
		fflush(netMsg);
		UsedNetMsg = TRUE;
	}
}

/*
 * netInfo()
 *
 * This function acquires necessary info from the user when entering a message.
 */
char netInfo(char GetName)
{
    int    cost, flags;
    label  domain = "";
    char   sys[NAMESIZE * 2];
    char   isdomain, *address;
    extern char *NoCredit;
    char   work[45];

    if (thisRoom == MAILROOM) {
	strcpy(sys, msgBuf.mbaddr);
	flags = 0;
	if (GetName) flags |= RNN_ASK;
	if (aide) flags |= RNN_WIDESPEC;
	if (!ReqNodeName("system to send to", sys, domain, flags, &netBuf))
	    return FALSE;

	isdomain = (domain[0] != 0);

	if (strCmpU(sys, ALL_LOCALS) != SAMESTRING) {
	    if (strCmpU(domain, cfg.nodeDomain + cfg.codeBuf) == SAMESTRING &&
		(strCmpU(sys, cfg.nodeName + cfg.codeBuf) == SAMESTRING ||
		strCmpU(sys, UseNetAlias(cfg.nodeName+cfg.codeBuf, TRUE))
							== SAMESTRING)) {
		mPrintf("Hey, that's this system!\n ");
		return FALSE;
	    }
	    cost = (isdomain) ? FindCost(domain) : !netBuf.nbflags.local;
	    if (logBuf.credit < cost) {
		if (HalfSysop()) {
		    logBuf.credit += cost;
		}
		else {
		    mPrintf(NoCredit);
		    return FALSE;
		}
	    }
	    if (isdomain) {
		sprintf(work, "%s _ %s", sys, domain);
		address = work;
	    }
	    else address = sys;

	    if (!isdomain && netBuf.nbflags.OtherNet) {
		sprintf(work, "%s address", netBuf.netName);
		getNormStr(work, msgBuf.mbOther, O_NET_PATH_SIZE, 0);
		if (strlen(msgBuf.mbOther) == 0) return FALSE;
	    }
	}
	else {
	    address = ALL_LOCALS;
	}
    }
    else {
	if (!roomBuf.rbflags.SHARED) {
	    mPrintf("This is not a network room\n ");
	    return FALSE;
	}
	address = R_SH_MARK;
	strcpy(msgBuf.mboname, cfg.codeBuf + cfg.nodeName);
	strcpy(msgBuf.mbdomain, cfg.codeBuf + cfg.nodeDomain);
    }
    strcpy(msgBuf.mbaddr, address);
    return TRUE;
}

/*
 * killConnection()
 *
 * Zaps carrier for network.
 */
void killConnection(char *s)
{
    HangUp(TRUE);
    modStat = haveCarrier = FALSE;
    while (MIReady()) Citinp();    /* Clear buffer of garbage */
}

/*
 * Phone Co is changing the rules in some parts of North America, so ...
 * see command line parser in main()
 */
char LocalAreaCode = 0;
/*
 * makeCall()
 *
 * This handles the actual task of dialing the modem.
 */
int makeCall(char EchoErr, MenuId id)
{
    char  call[80];
    label blip1;
    int   bufc, result;
    char  buf[30], c, viable;
    char  ourArea[4], targetArea[4];

    while (MIReady()) Citinp();

    AreaCode(netBuf.netId, targetArea);
    AreaCode(cfg.nodeId + cfg.codeBuf, ourArea);

    if (!netBuf.nbflags.ExternalDialer) {
	setNetCallBaud(netBuf.baudCode);
	normId(netBuf.netId, blip1);

#ifdef POSSIBLE
	strcpy(call, cfg.codeBuf + cfg.DialPrefixes[minimum(netBuf.baudCode, cfg.sysBaud)]);
#else
	strcpy(call, GetPrefix(minimum(netBuf.baudCode, cfg.sysBaud)));
#endif

	if (strlen(netBuf.access) != 0) {	/* don't need to check extdial*/
	    strcat(call, netBuf.access);
	}
	else if (!netBuf.nbflags.local) {
	    strcat(call, "1");

	    /* LD within same area code? (courtesy farokh irani) */
	    if (strCmp(targetArea, ourArea) == SAMESTRING && !LocalAreaCode)
		strcat(call, blip1 + 5);
	    else
		strcat(call, blip1 + 2);
	}
	else {
	    /* local but different area codes?  (e.g., NYC) */
	    /* again courtesy farokh irani */
	    if (strCmp(targetArea, ourArea) != SAMESTRING)
		strcat(call, blip1 + 2);
	    else
		strcat(call, blip1 + 5);
	}
	strcat(call, cfg.codeBuf + cfg.netSuffix);

	switch (RottenDial(call)) {
	    case FALSE: moPuts(call); break;
	    case TRUE:  break;
	    case ERROR: return FALSE;
	}

	for (startTimer(WORK_TIMER), bufc = 0, viable = TRUE;
	    chkTimeSince(WORK_TIMER) < ((netBuf.nbflags.local) ? 40l :
								cfg.LD_Delay)
							&& viable;) {
	    if (gotCarrier()) break;
		/* Parse incoming string from modem -- call progress detection */
	    if (KBReady()) viable = FALSE;
	    if (MIReady()) {
		if ((c = Citinp()) == '\r') {
		    buf[bufc] = 0;
		    switch ((result = ResultVal(buf))) {
			case R_NODIAL:
			case R_NOCARR:
			case R_BUSY:
			    if (EchoErr) splitF(netLog, "(%s) ", buf);
			    else SysopPrintf(id, "\n%s", buf);
			    viable = FALSE; break;
			case R_300:
			case R_1200:
			case R_2400:
			case R_4800:
			case R_9600:
			case R_14400:
			case R_19200:
			    if ((minimum(netBuf.baudCode, cfg.sysBaud)) != result) {
				setNetCallBaud(result);
				splitF(netLog, "(Mismatch: %s, adjusting.)\n", buf);
			    }
			    break;
		    }
		    bufc = 0;
		}
		else {
		    if (bufc > 28) bufc = 0;
		    else if (c != '\n') {
			buf[bufc++] = c;
		    }
		}
	    }
	}
	if (gotCarrier())
	    return TRUE;
    }
    else {
	return DialExternal(&netBuf);
    }
    return FALSE;
}

#ifdef LUNATIC
ExplainNeed(int i, MULTI_NET_DATA x)
{
    splitF(netLog, "slot %d i%d isp%d sp%d MN%ld nm%d rf%d sf%d shared%d\n", i,
	  netTab[i].ntflags.in_use,
	  netTab[i].ntflags.is_spine,
	  netTab[i].ntflags.spine,
	  netTab[i].ntMemberNets & x, netTab[i].ntflags.normal_mail,
	  netTab[i].ntflags.room_files,
	  netTab[i].ntflags.send_files, roomsShared(i));
    splitF(netLog, "DF%d HR%d anyr%d\n",
	    DomainFlags[i], netTab[i].ntflags.HasRouted, AnyRouted(i));
}
#endif

#define SpineSet(i) (netTab[i].ntflags.is_spine && !(netTab[i].ntMemberNets & PRIORITY_MAIL))
/*
 * needToCall()
 *
 * This is responsible for checking to see if we need to call this system.
 * Basically, here's what the rules are:
 *
 * Is this account in use?
 * Is this account on one of the eligible net ('x' parameter)?
 * Is this account not a spine and not an OtherNet system?
 *
 * If this system is a spine and we're not doing an anytime-net session
 * and we haven't had a successful connection yet then call.
 *
 * If this system has normal mail, room file requests, send file requests,
 * rooms that need to share (outgoing messages), domain mail outgoing, mail
 * routing and hasn't been connected with yet, then call.
 * 
 * I'm not entirely sure what the reference to Priority Mail signifies in
 * this mess.
 */
int needToCall(int i, MULTI_NET_DATA x)
{
	extern SListBase Routes;
	SystemCallRecord *called;

	called = SearchList(&SystemsCalled, &i);

	/*
	 * Only call once per network session
	 */
	if ((        inNet == NORMAL_NET ||
	             inNet == ANYTIME_NET ||
		     inNet == UNTIL_NET)
		            &&
	    (        called != NULL &&
	             called->Status == SYSTEM_CALLED       )) {
		return FALSE;
	}
				/* first check for permission to call   */
	if (netTab[i].ntflags.in_use &&     /* account in use		*/
		(netTab[i].ntMemberNets & x) &&  /* system is member of net */
		!SpineSet(i) &&
		!netTab[i].ntflags.OtherNet) {   /* system not OtherNet	*/
				/* check for requirement to call	*/
		if (called != NULL && called->Unstable >= cfg.MaxNotStable) {
			return FALSE;
		}
		if (netTab[i].ntflags.spine &&
				!(netTab[i].ntMemberNets & PRIORITY_MAIL) &&
		   		(inNet == NON_NET || ((inNet == NORMAL_NET ||
				inNet == UNTIL_NET) &&
				(called == NULL ||
					called->Status != SYSTEM_CALLED)))) {
			return TRUE;
		}
				/* now check for need to call     */
		if (
			(SearchList(&Routes, &i) == NULL &&
			netTab[i].ntflags.normal_mail ||/* normal out mail?*/
			netTab[i].ntflags.HasRouted) ||
			netTab[i].ntflags.room_files ||	/* request files ? */
			netTab[i].ntflags.send_files ||	/* send files ?	*/
			DomainFlags[i] ||	/* domain mail to send?	*/
			roomsShared(i) ||		/* rooms to share? */
			AnyRouted(i) ||
			(netTab[i].ntflags.HasRouted &&
			(inNet == NON_NET || 
			((inNet == NORMAL_NET || inNet == ANYTIME_NET ||
				inNet == UNTIL_NET) &&
				(called == NULL ||
				called->Status != SYSTEM_CALLED))))) {
#ifdef NEEDED
printf("\nmail=%d, HR=%d, SL=%lx, AnyRouted=%d\n",
netTab[i].ntflags.normal_mail,
netTab[i].ntflags.HasRouted,
SearchList(&Routes, &i), AnyRouted(i));
#endif
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * AnyCallsNeeded()
 *
 * Do we need to make any calls 'tall?
 */
char AnyCallsNeeded(MULTI_NET_DATA whichNets)
{
    int searcher;

    for (searcher = 0; searcher < cfg.netSize; searcher++)
	if (needToCall(searcher, whichNets)) return TRUE;

    return FALSE;
}

/*
 * ReqNodeName()
 *
 * This function is a general request for node name from user.  It supports
 * various options for prompting or not prompting for the name, allowing the
 * input of '&L', allow display of nodelists, etc. (see the code).  The
 * function will validate the choice, query again if appropriate, handle
 * domains, and do a getNet of the node if necessary.
 */
char ReqNodeName(char *prompt, label target, label domain, int flags,
							NetBuffer *nBuf)
{
    extern char *ALL_LOCALS;
    char sysname[2 * NAMESIZE], dup, work[2 * NAMESIZE];
    int  slot;

    do {
	slot = ERROR;
	if (domain != NULL) domain[0] = 0;
	/* Allows function to act as validator only */
	if ((flags & RNN_ASK)) {
	    getString(prompt, sysname, 2 * NAMESIZE, QUEST_SPECIAL);
	}
	else strcpy(sysname, target);

	NormStr(sysname);

	/* Empty line implies operation abort. */
	if (strlen(sysname) == 0) {
	    strcpy(target, sysname);
	    return FALSE;
	}

	/* If "&L" entered and is acceptable ... */
	if ((flags & RNN_WIDESPEC) && strCmpU(sysname, ALL_LOCALS) == SAMESTRING) {
	    strcpy(target, sysname);
	    return TRUE;
	}

	/* Questioning frown */
	if (sysname[0] == '?') {
	    writeNet((flags & RNN_DISPLAY), FALSE);   /* write out available nets */
	    if ((flags & RNN_WIDESPEC)) 
		mPrintf("'&L' == Local Systems Announcement\n ");
	}
	/* finally, must be real system name so seeeeearch for it! */
	else if ((slot = searchNameNet(sysname, nBuf)) != ERROR) {
	    strcpy(target, nBuf->netName);	/* aesthetics */
	    if (nBuf->nbflags.local || nBuf->nbflags.RouteLock) {
		return TRUE;		/* Yup */
	    }
	}
	if (domain != NULL && SystemInSecondary(sysname, domain, &dup)) {
	    if (dup) {	/* oops */
		if (slot != ERROR) return TRUE;
			/* do it as a double if, not claused */
		if ((flags & RNN_ASK)) mPrintf(DupDomain, sysname);
	    }
	    else {
		strcpy(target, sysname);	/* aesthetics */
		return TRUE;
	    }
	}
	if (slot != ERROR) return TRUE;
	if (sysname[0] != '?') {
	    if (!(flags & RNN_QUIET)) {
		sprintf(work, "%s not listed.\n", sysname); /* Nope */
		if ((flags & RNN_SYSMENU)) SysopError(NO_MENU, work);
		else	   mPrintf(work);
	    }
	}
    } while (!(flags & RNN_ONCE) && (flags & RNN_ASK)); /* This controls if we ask repeatedly or only once */

    return FALSE;       /* And if we get here, we definitely are a failure */
}

/*
 * NetValidate()
 *
 * This will return TRUE if net privs are go, FALSE otherwise.
 */
char NetValidate(char talk)
{
    if (!cfg.BoolFlags.netParticipant) {
	if (talk)
	    mPrintf("This Citadel is not participating in the net.\n ");
	return FALSE;
    }

    if ( !loggedIn ||
		(!logBuf.lbflags.NET_PRIVS &&
		(!roomBuf.rbflags.AUTO_NET || !roomBuf.rbflags.ALL_NET) )) {
	if (talk) 
	    mPrintf("\n Sorry, you don't have net privileges.\n ");
	return FALSE;
    }
    return TRUE;
}

/*
 * FindRouteIndex()
 *
 * This will find the next route filename in sequence.
 */
int FindRouteIndex(int slot)
{
    label temp;
    SYS_FILE newfn;

    sprintf(temp, "R%d", slot);
    makeSysName(newfn, temp, &cfg.netArea);

    return FindNextFile(newfn);
}

typedef struct {
    int count;
    char *str;
} SR_Arg;
/*
 * ParticipatingNodes()
 *
 * This function prepares a string indicating who shares the current room with
 * us.
 */
void ParticipatingNodes(char *target)
{
	SR_Arg arg;
	char   *c;
	int ShowSharedRoomName(SharedRoomData *room, int system, 
						int roomslot, void *arg);

	sprintf(lbyte(target), ". This room is shared with: ");
	arg.count = 0;
	arg.str   = target;
	EachSharingNode(thisRoom, 0, ShowSharedRoomName, &arg);

	if (arg.count) {	/* this eliminates the trailing comma */
		c = lbyte(target);
		c -= 2;
		*c = 0;
	}
}

/*
 * ShowSharedRoomName()
 *
 * This appends the name of this shared room to a string.
 */
int ShowSharedRoomName(SharedRoomData *room, int system, int roomslot, void *d)
{
	SR_Arg *arg;
	char *name;
	char commnd, *s1, *s2, *s3, doit;

	arg = d;
	getNet(system, &netBuf);
	arg->count++;
	Addressing(system, room->room, &commnd, &s1, &s2, &s3, &name, &doit);
	sprintf(lbyte(arg->str), "%s (%s), ", netBuf.netName, name);

	return TRUE;
}

/*
 * AreaCode()
 *
 * This function extracts the area code from the node id.
 */
void AreaCode(char *Id, char *Target)
{
	int i, j;

	for (i = j = 0; j < 3 && Id[i]; i++)
		if (isdigit(Id[i]))
			Target[j++] = Id[i];

	Target[j] = 0;
}

/*
 * NetInit()
 *
 * This function does network initialization: Cache handling, network recovery
 * (in case of crash during netting), etc...
 */
void NetInit()
{
    int rover;
    extern SListBase Routes;
    SYS_FILE filename;

    SpecialMessage("Network Initialization");
    VirtInit();
    VortexInit();

    /* we never need do this again */
    makeSysName(filename, "routing.sys", &cfg.netArea);
    MakeList(&Routes, filename, NULL);

    makeSysName(filename, "shutup.sys", &cfg.netArea);
    MakeList(&Shutup, filename, NULL);

    DomainInit(TRUE);

    for (rover = 0; rover < cfg.netSize; rover++)
	if (netTab[rover].ntMemberNets & PRIORITY_MAIL) PriorityMail++;

    RecoverNetwork();

    SpecialMessage("");
}

/*
 * MakeNetted()
 *
 * This function will make a message into a net message.  This is userland
 * code called when an aide wants to make a non-netted message into a netted
 * message.
 */
char MakeNetted(int m, char tonet)
{
	if (findMessage(roomBuf.msg[m].rbmsgLoc,roomBuf.msg[m].rbmsgNo,TRUE)) {
		getMsgStr(getMsgChar, msgBuf.mbtext, MAXTEXT);/* get balance */
		if (tonet) {
			strcpy(msgBuf.mboname, cfg.codeBuf + cfg.nodeName);
			strcpy(msgBuf.mbdomain, cfg.codeBuf + cfg.nodeDomain);
			strcpy(msgBuf.mbaddr, R_SH_MARK);
		}
		else {
			msgBuf.mbaddr[0] = 0;
			msgBuf.mboname[0] = 0;
			msgBuf.mbdomain[0] = 0;
		}
		DelMsg(TRUE, m);
		putMessage(NULL, 0);
		return NETTED;
	}
	return NO_CHANGE;
}

/*
 * freeUNS()
 *
 * This function is purportedly a free function for a list.  In actuality,
 * it also runs a net session for each.
 */
void freeUNS(TwoNumbers *netdata)
{
    int yr, dy, hr, mn, mon, secs, milli;

    if (!onLine()) {
	getRawDate(&yr, &mon, &dy, &hr, &mn, &secs, &milli);
	netController((hr * 60) + mn, (int) netdata->second,
			(1l << (netdata->first - 1)), UNTIL_NET, 0);
    }
    free(netdata);
}

/*
 * HasOutgoing()
 *
 * This function checks to see if the given room as specified by the system,
 * index pair has outgoing messages in either cache or message form.
 *
 * NB: This code may not be OK.
 */
char HasOutgoing(SharedRoom *room)
{
    if (GetFA(room->mode)) return TRUE;
    if (roomTab[netRoomSlot(room)].rtlastMessage > room->lastMess)
	return TRUE;
    return FALSE;
}

/*
 * FindCalledNode()
 *
 * This function helps find a SystemCallRecord in a list by comparing the
 * given integer (*node) with the node id of the record.  Returns the record
 * on match, NULL otherwise.
 */
static void *FindCalledNode(SystemCallRecord *system, int *node)
{
	if (*node == system->Node)
		return system;
	return NULL;
}
