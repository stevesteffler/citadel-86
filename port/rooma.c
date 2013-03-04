/*
 *				rooma.c
 *
 * room code for Citadel bulletin board system.
 */

/*
 *				history
 *
 *	SEE THE INCREM.* FILES FOR FURTHER HISTORICAL NOTES
 * 84Jul12 JLS & HAW  gotoRoom() and dumpRoom() modified for <S>kip.
 * 84Apr04 HAW  Start 1.50a update
 * 83Feb24      Insert check for insufficient RAM, externs too low.
 * 82Dec06 CrT  2.00 release.
 * 82Nov05 CrT  main() splits off to become citadel.c
 */

#include "ctdl.h"

/*
 *				Contents
 *
 *	CheckForSkippedMsgs()	check for skipped msgs (Mail)
 *	CountMsgs()		counts the messages in the room.
 *	DateSearch()		analyzes for date specs
 *	dumpRoom()		tells us # new messages etc
 *	fillMailRoom()		set up Mail> from log record
 *	gotoRoom()		handles "g(oto)" command for menu
 *	GotoNamedRoom()		goto the named room, if possible.
 *	initCitadel()		system startup initialization
 *	KnownRoom()		is room known?
 *	knowRoom()		does some user know of specified room?
 *	legalMatch()		Looks for partial matches.
 *	listRooms()		lists known rooms
 *	partialExist()		partial roomname matcher.
 *	retRoom()		handle Ungoto command
 *	roomCheck()		returns slot# of named room else ERROR
 *	roomExists()		returns slot# of named room else ERROR
 *	searchRooms()		searches room list for matching string
 *	setUp()			setup the log buffer vars correctly
 *	SkippedNewRoom()	worker function
 *	systat()		shows current system status
 *	tableRunner()		Applies some function to all the rooms.
 *	UngotoMaintain()	Maintains the Ungoto list.
 */

char		*baseRoom, BadMessages[40], IgnoreDoor;
int		UngotoStack[UN_STACK];
char		remoteSysop = FALSE;      /* Is current user a sysop      */
char		shownHidden;
MSG_NUMBER	*lPtrTab;			/* For .Ungoto		*/
char		BpsSet = FALSE;
#ifdef B_HERE
SListBase	BadWords = { NULL, FindIcky, NULL, NULL, EatIcky };
#endif
UNS_16		*RoomMsgCount;
int		pgdft;	/* default to 0 */
SListBase	ChatOn = { NULL, FindStr, NULL, NULL, EatIcky };

extern CONFIG    cfg;		/* A buncha variables		*/
extern LogTable  *logTab;	/* RAM index of pippuls		*/
extern MessageBuffer   msgBuf;
extern logBuffer logBuf;	/* Pippul buffer		*/
extern logBuffer logTmp;	/* Pippul buffer		*/
extern NetBuffer netBuf;
extern NetBuffer netTemp;
extern struct floor     *FloorTab;
extern FILE      *logfl;		/* log file descriptor		*/
extern FILE      *netfl;		/* Net file		*/
extern rTable    *roomTab;	/* RAM index of rooms		*/
extern aRoom     roomBuf;	/* room buffer		*/
extern FILE      *roomfl;	/* file descriptor for rooms    */
extern int       thisRoom;	/* room currently in roomBuf    */
extern char      loggedIn;	/* Are we logged in?		*/
extern char      PrintBanner;
extern char      echo;		/* output flag		*/
extern char      prevChar;	/* Last char out		*/
extern char      onConsole;	/* on console?		*/
extern char      whichIO;	/* where is the I/O?		*/
extern int       thisSlot;	/* Current log slot		*/
extern char      outFlag;
extern char      nextDay;	/* System up before bailout?    */
extern char      heldMess;
extern label     oldTarget;	/* Room to move messages to     */
extern char      ShowNew;
extern char      JustChecking;
extern SListBase Moderators;

/*
 * DateSearch()
 *
 * This function analyzes for possible"[<|>] <date-spec>" in string.
 */
char *DateSearch(char *str, long *before, long *after)
{
    long *which;

    while (*str && *str != '>' && *str != '<')
	str++;

    switch (*str) {
	case '>':
	    which = after; break;
	case '<':
	    which = before; break;
	default: return NULL;
    }

    /* part of our duties is to put EOS here */
    *str++ = 0;
    while (*str && *str == ' ') str++;
    if (*which == -1l) {	/* if not set yet */
	if (strlen(str))
	    ReadDate(str, which);
	else
	    *which = logBuf.lblaston;
    }
    /* now we need to find the possible location of the next date spec */
    /* first, skip over current date. */
    while (*str && *str != ' ' && *str != '>' && *str != '<') str++;
    return str;
}

/*
 * dumpRoom()
 *
 * This will tell us # new messages etc.
 */
void dumpRoom(char ShowFloor)
{
    extern char HasSkipped;
    int		count, newCount;

    CountMsgs(&count, &newCount);

    if (!loggedIn && thisRoom == MAILROOM)      /* Kludge for new users */
	newCount = count = 1;			/* So they see intro.   */

    if (ShowFloor)
	mPrintf("  [%s]\n ", FloorTab[thisFloor].FlName);

    mPrintf(" %d messages\n ", count);
    if (newCount > 0 && !PrintBanner)
	mPrintf(" %d new\n", newCount);

    if (thisRoom == LOBBY) {
	HasSkipped = FALSE;
	if (tableRunner(NSRoomHasNew, TRUE) != ERROR)
	    return ;

	if (tableRunner(RoomHasNew, TRUE) == ERROR)
	    return ;

	if (HasSkipped) {
	    if (FloorMode)
		FSkipped();
	    else {
		mPrintf("\n Skipped rooms: \n ");
		ShowNew = TRUE;
		JustChecking = FALSE;
		tableRunner(SkippedNewRoom, TRUE);
	    }
	}
    }
}

/*
 * CountMsgs()
 *
 * This function counts the messages in the room, total and new.
 */
void CountMsgs(int *count, int *newCount)
{
    int i;

    MSG_NUMBER msgNo;

#ifdef NEEDED
PagingOn();
mPrintf("thisRoom=%d, lastVisit=%ld\n ", thisRoom, logBuf.lastvisit[thisRoom]);
#endif
    for (*newCount = *count = i = 0;  
	     i < ((thisRoom == MAILROOM) ? MAILSLOTS : MSGSPERRM);   i++) {
				/* Msg is still in system?  Count it.   */
	msgNo = roomBuf.msg[i].rbmsgNo & S_MSG_MASK;

	if (msgNo >= cfg.oldest) {
	    (*count)++;

	    /* don't boggle -- just checking against newest as of */
	    /* the last time we were  in this room - also against */
	    /* msg skip bit.					  */
	    if ((msgNo > logBuf.lastvisit[thisRoom] &&
		msgNo <= cfg.newest) || msgNo != roomBuf.msg[i].rbmsgNo) {
#ifdef NEEDED
mPrintf("%3d. %ld, %ld\n ", i, msgNo, roomBuf.msg[i].rbmsgNo);
#endif
		(*newCount)++;
	    }
	}
    }
#ifdef NEEDED
mPrintf("count is %d, newCount is %d\n ", *count, *newCount);
PagingOff();
#endif
}

/*
 * SkippedNewRoom()
 *
 * This is used for calls to tableRunner(), clears room's SKIP flag, if
 * ShowNew is TRUE it prints the room's name, else just return TRUE.
 */
int SkippedNewRoom(int i)
{
    if (roomTab[i].rtflags.SKIP == 1 && RoomHasNew(i)) {
	roomTab[i].rtflags.SKIP = 0;	/* Clear. */
	if (ShowNew) mPrintf(" %s ", formRoom(i, TRUE, TRUE));
	if (JustChecking) return TRUE;
    }
    return FALSE;
}

/*
 * fillMailRoom()
 *
 * This fills up the Mail room.
 */
void fillMailRoom()
{
    memcpy(roomBuf.msg, logBuf.lbMail, MAIL_BULK);
    noteRoom();
}

/*
 * gotoRoom()
 *
 * This is the menu fn to travel to a new room.
 * returns TRUE if room is Lobby>, else FALSE.
 */
int gotoRoom(char *nam, int flags)
{
	int  i, foundit, roomNo, s;
	int  lRoom, oldFloor;
	static char *noRoom = " ?no %s room\n";
	char NewFloor = FALSE;

	lRoom = thisRoom;

	if (strLen(nam) == 0) {

		foundit = FALSE;  /* leaves us in Lobby> if nothing found */
		if (!(flags & MOVE_SKIP)) {
			SetKnown(-1, thisRoom, &logBuf);
			logBuf.lastvisit[thisRoom] = cfg.newest;
			roomTab[thisRoom].rtflags.SKIP = CheckForSkippedMsgs();
		}
		if (!FloorMode) {

			for (i = 0; i<MAXROOMS  &&  !foundit; i++) {
				s = knowRoom(&logBuf, i);
				if (
					(s == KNOW_ROOM || s == WRITE_PRIVS ||
					(s != DEAD_ROOM &&
					aide && cfg.BoolFlags.aideSeeAll &&
					(!roomTab[i].rtflags.INVITE ||
					SomeSysop()))) &&
					!roomTab[i].rtflags.SKIP
				) {
					if (roomTab[i].rtlastMessage > logBuf.lastvisit[i] &&
						roomTab[i].rtlastMessage >= cfg.oldest) {
						if (i != thisRoom) {
							foundit  = i;
						}
					}
				}
			}

			getRoom(foundit);
			if (flags & MOVE_TALK)
				mPrintf("%s\n ", roomBuf.rbname);
		}
		else {
			NewFloor = NewRoom(flags);
			foundit = thisRoom;
		}
		UngotoMaintain(lRoom);
	} else {
		foundit = 0;
		oldFloor = thisFloor;
		if ((roomNo = GotoNamedRoom(nam, flags)) == ERROR)
			mPrintf(noRoom, nam);
		else {
			foundit = roomNo;
			if (FloorMode) NewFloor = !(oldFloor == thisFloor);
		}
	}

	setUp(FALSE);
	if (flags & MOVE_TALK) dumpRoom(NewFloor);

	/* in case recover1 gets a room back for a non-existent floor */
	if (!FloorTab[roomBuf.rbFlIndex].FlInuse)
		roomBuf.rbFlIndex = 0;

	return foundit;
}

/*
 * GotoNamedRoom()
 *
 * This function will goto the named room, if possible.
 */
int GotoNamedRoom(char *name, int flags)
{
    int roomNo;

    if ((roomNo = RealGNR(name, roomExists)) == ERROR &&
    	(roomNo = RealGNR(name, partialExist)) == ERROR)
	return ERROR;
    if (roomNo != thisRoom) {
	if (!(flags & MOVE_SKIP)) {
	    SetKnown(-1, thisRoom, &logBuf);
	    logBuf.lastvisit[thisRoom] = cfg.newest;
	    roomTab[thisRoom].rtflags.SKIP = CheckForSkippedMsgs();
	}
	UngotoMaintain(thisRoom);
	getRoom(roomNo);

	/* if may have been unknown... if so, note it:      */
	if (!KnownRoom(thisRoom)) {
	    SetKnown(0, thisRoom, &logBuf);
	}
    }
    return roomNo;
}

/*
 * RealGNR()
 *
 * This function does the real work of checking to see if a .Goto is legal.
 */
int RealGNR(char *nam, int (*func)(char *room))
{
    int roomNo;

    /* non-empty room name, so now we look for it: */
    if ((roomNo = roomCheck(func, nam)) == ERROR ||
    	roomTab[roomNo].rtflags.INVITE && !SomeSysop() &&
		roomTab[roomNo].rtgen != logBuf.lbrgen[roomNo] &&
		abs(roomTab[roomNo].rtgen - logBuf.lbrgen[roomNo])
						!= RO_OFFSET) {
	return ERROR;
    } else {
	return roomNo;
    }
}

/*
 * initCitadel()
 *
 * This initializes system, returns TRUE if system is coming up normally,
 * false if returning from a door call.
 */
char initCitadel()
{
	SYS_FILE    tempName;
	extern char ExitToMsdos, *DirFileName;
	extern UNS_32  BaudRate;
	extern char *READ_TEXT, *VERSION;
	int		SysVal;
	char	fromDoor;
	extern SListBase Arch_base, DirBase;
	extern char justLostCarrier;
	extern FILE *upfd;
	extern int  IckyLevel;

	echo = BOTH;

	if (!readSysTab(TRUE, TRUE))
		exit(CRASH_EXIT);/* No system table? Tacky, tacky*/

	cfg.weAre = CITADEL;
	if ((SysVal = systemInit()) != 0) {
		writeSysTab();
		systemShutdown(SysVal);
		exit(CRASH_EXIT);
	}

	printf("\n%s V%s\n%s\n\n", VARIANT_NAME, VERSION, COPYRIGHT);
	printf("This software is Public Domain, not Commercial and not Shareware.\n\n");
	printf("IF YOU PAID FOR THIS SOFTWARE, SOMEONE IS RIPPING YOU OFF.\n\n");

	if (access(LOCKFILE, 0) != -1) {
		printf("Lock File found!!  Do you have Citadel already up?\n");
		writeSysTab();	/* Save it out just in case */
		systemShutdown(0);
		exit(RECURSE_EXIT);
	}

	SpecialMessage("Opening files");
	/* open message files: */
	InitMsgBase();

	initLogBuf(&logBuf);
	initLogBuf(&logTmp);
	initRoomBuf(&roomBuf);
	initNetBuf(&netBuf);
	initNetBuf(&netTemp);
	initSharedRooms(FALSE);
	initTransfers();
	ReadCitInfo();
	lPtrTab = (MSG_NUMBER *) GetDynamic(MAXROOMS * sizeof (MSG_NUMBER));
	RoomMsgCount = GetDynamic(MAXROOMS * sizeof *RoomMsgCount);

	strCpy(oldTarget, "Aide");

	baseRoom = &cfg.codeBuf[cfg.bRoom];

	setUp(TRUE);

	InitIgnoreMail();

	makeSysName(tempName, "ctdllog.sys",  &cfg.logArea);
	openFile(tempName, &logfl );
	makeSysName(tempName, "ctdlroom.sys", &cfg.roomArea);
	openFile(tempName, &roomfl);

	makeSysName(tempName, "ctdlarch.sys", &cfg.roomArea);
	MakeList(&Arch_base, tempName, NULL);

	makeSysName(tempName, DirFileName, &cfg.roomArea);
	MakeList(&DirBase, tempName, NULL);

	makeSysName(tempName, "chaton.sys", &cfg.roomArea);
	MakeList(&ChatOn, tempName, NULL);

	OffLineInit();

#ifdef B_HERE
	/* Icky level is the first line of Badwords, filename is second line */
	makeSysName(tempName, "badwords.sys", &cfg.roomArea);
	if ((upfd = fopen(tempName, READ_TEXT)) != NULL) {
		if (GetAString(msgBuf.mbtext, MAXTEXT, upfd) != NULL)
			IckyLevel = atoi(msgBuf.mbtext);
		if (GetAString(msgBuf.mbtext, MAXTEXT, upfd) != NULL)
			strCpy(BadMessages, msgBuf.mbtext);
		MakeList(&BadWords, "", upfd);
	}
#endif

	makeSysName(tempName, "ctdlmodr.sys", &cfg.roomArea);
	MakeList(&Moderators, tempName, NULL);

	if (cfg.BoolFlags.netParticipant) {
		makeSysName(tempName, "ctdlnet.sys", &cfg.netArea);
		openFile(tempName, &netfl);
		NetInit();
		OpenForwarding();
	}

	getRoom(LOBBY);     /* load Lobby>  */

	fromDoor = BackFromDoor();

	InitEvents(TRUE);

	if ((!IgnoreDoor && cfg.BoolFlags.IsDoor) && !fromDoor && !BpsSet) {
		printf("This is a Door C-86.\n");
		writeSysTab();
		exit(RECURSE_EXIT);
	}

	/* Now open the modem up */
	SpecialMessage("Modem initialization");
	ModemOpen((gotCarrier() && fromDoor) ||
				(cfg.BoolFlags.IsDoor && !IgnoreDoor));

	ExitToMsdos = !ModemSetup((fromDoor ||
		(cfg.BoolFlags.IsDoor && !IgnoreDoor)) && BaudRate != 0l);
	SpecialMessage("");

	if (!cfg.BoolFlags.IsDoor || IgnoreDoor) ExitToMsdos = FALSE;

	if ((fromDoor || BpsSet) && BaudRate == 0l) {
		DisableModem(FALSE);
		whichIO = CONSOLE;
	}
	else
		whichIO = MODEM;

	setUp(FALSE);

	if (fromDoor && BaudRate != 0l && !gotCarrier())
		justLostCarrier = TRUE;

	return !fromDoor;	/* if we come back from a door, don't   */
				/* display a banner.			*/
}

/*
 * legalMatch()
 *
 * This looks for partial matches, checks legalities.
 */
char legalMatch(int i, label target)
{
    char  Equal, *endbuf;

    Equal = KnownRoom(i);

    if ((roomTab[i].rtflags.INUSE == 1) &&
		((aide && cfg.BoolFlags.aideSeeAll &&
		!roomTab[i].rtflags.INVITE)
		|| Equal)) {
	endbuf = lbyte(roomTab[i].rtname);
	return (matchString(roomTab[i].rtname, target, endbuf) != NULL);
    }
    return FALSE;
}

/*
 * listRooms()
 *
 * This function lists known rooms.
 */
void listRooms(char mode)
{
    extern char SelDirs, SelShared, SelPriv, SelNew, SelAnon, NotForgotten;
    extern char SelRO;

    shownHidden = FALSE;
    switch (mode) {
	case DR_SEL:   SelDirs = TRUE; break;
	case SH_SEL:   SelShared = TRUE; break;
	case PR_SEL:   SelPriv = TRUE; break;
	case ANON_SEL: SelAnon = TRUE; break;
	case READONLY: SelRO = TRUE; break;
	case INT_EXPERT:
	case INT_NOVICE:
	case NOT_INTRO:
			SelNew = TRUE; break;
	case FORGOTTEN:
			NotForgotten = FALSE;
			SelNew = TRUE;
			break;
    }

    if (FloorMode) {
	FKnown(mode);
    }
    else {    /* Else */
	if (SelNew && NotForgotten) {
	    mPrintf("\n Rooms with unread messages:\n ");
	    ShowNew = 1;
	}
	else if (mode == FORGOTTEN) {
	    mPrintf("\n Forgotten public rooms:\n ");
	    ShowNew = 2;
	}
	tableRunner(DispRoom, TRUE);
	if (mode != INT_EXPERT && SelNew && NotForgotten) {
	    mPrintf("\n No unseen msgs in:\n ");
	    ShowNew = FALSE;
	    tableRunner(DispRoom, TRUE);
	}
    }
    SelDirs = SelShared = SelRO = SelPriv = SelNew = SelAnon = FALSE;
    NotForgotten = TRUE;
}

/*
 * tableRunner()
 *
 * This applies some function to all the rooms the user might know of.
 *
 * OnlyKnown: decides if every room is subject to the function call, or only
 * those the current user knows of.
 */
int tableRunner(int (*func)(int rover), char OnlyKnown)
{
    int rover;

    for (rover = 0; rover < MAXROOMS; rover++) {
	if (!OnlyKnown || KnownRoom(rover))
	    if ((*func)(rover)) return rover;
    }
    return ERROR;
}

/*
 * KnownRoom()
 *
 * This is called by tableRunner, returns whether room is known.  External flag
 * NotForgotten controls if we're doing a normal Known rooms or a list of
 * ZForgotten rooms.
 */
int KnownRoom(int RoomNo)
{
    extern char NotForgotten;
    int s;

    s = knowRoom(&logBuf, RoomNo);
    if (NotForgotten) {
	return (s == KNOW_ROOM || s == WRITE_PRIVS);
    }

	/* now checking for Forgotten rooms -- don't show if private room! */
    if (!roomTab[RoomNo].rtflags.PUBLIC) return FALSE;

    return (s == FORGOTTEN_ROOM);
}

/*
 * knowRoom()
 *
 * This will check to see if specified user knows given room.
 *
 * Return 0 if not know room, 2 if forgot room, 3 if know room and have write
 * permission (r/o rooms), 1 otherwise.
 */
char knowRoom(logBuffer *lBuf, int i)
{
    int difference;

    if (!roomTab[i].rtflags.INUSE) return DEAD_ROOM;
    difference = abs(roomTab[i].rtgen - lBuf->lbrgen[i]);
    return	((difference == 0) ? KNOW_ROOM :
		(difference == RO_OFFSET) ? WRITE_PRIVS :
		((difference == FORGET_OFFSET) ? FORGOTTEN_ROOM :
		(roomTab[i].rtflags.PUBLIC) ? KNOW_ROOM : UNKNOWN_ROOM));
}

/*
 * SetKnown()
 *
 * This sets up a known-room value.
 */
void SetKnown(int GenVal, int Room, logBuffer *lBuf)
{
    int val;

    switch (GenVal) {
    case -2:
	val = (roomTab[Room].rtgen + (MAXGEN-1)) % MAXGEN;
	break;
    case -1:
	val = lBuf->lbrgen[Room];
	break;
    case 0:
	val = roomTab[Room].rtgen;
	break;
    case RO_OFFSET:
	val = (roomTab[Room].rtgen + RO_OFFSET) % MAXGEN;
	break;
    case FORGET_OFFSET:
	val = (roomTab[Room].rtgen + FORGET_OFFSET) % MAXGEN;
    default:
	break;
    }
    lBuf->lbrgen[Room] = val;
}

/*
 * partialExist()
 *
 * This roams the list looking for a partial match.
 */
int partialExist(label target)
{
    int rover;

    for (rover = (thisRoom + 1) % MAXROOMS; rover != thisRoom;
					rover = (rover + 1) % MAXROOMS)
	if (legalMatch(rover, target)) return rover;
    return ERROR;
}

/*
 * retRoom()
 *
 * This is the menu Ungoto command.
 */
void retRoom(char *roomName)
{
    int slot, OldFloor;

    OldFloor = thisFloor;
    if (strLen(roomName) == 0) {
	if (UngotoStack[0] == -1) {
	    mPrintf("\n No room to Ungoto to!\n ");
	    return;
	}
	getRoom(UngotoStack[0]);
	mPrintf("%s\n ", roomBuf.rbname);
	logBuf.lastvisit[thisRoom] = lPtrTab[thisRoom];
				/* Now pop that top element off the stack */
	memmove(UngotoStack, UngotoStack + 1, (UN_STACK - 1) * sizeof(int));
	UngotoStack[UN_STACK-1] = -1;   /* bottom of stack */
    }
    else {
	if (
	    (slot = RealGNR(roomName, roomExists)) == ERROR &&
	    (slot = RealGNR(roomName, partialExist)) == ERROR
	) {
	    mPrintf(" ?no %s room\n", roomName);
	    return;
	}
	UngotoMaintain(thisRoom);
	getRoom(slot);
	logBuf.lastvisit[thisRoom] = lPtrTab[thisRoom];
    }
    setUp(FALSE);
    dumpRoom((char) FloorMode ? !(OldFloor == thisFloor) : FALSE);
}

/*
 * roomCheck()
 *
 * This returns slot# of named room else ERROR as determined by checker.
 */
int roomCheck(int (*checker)(char *name), char *nam)
{
    int roomNo;

    if (
	(roomNo = (*checker)(nam)) == ERROR
	||
	(roomNo==AIDEROOM  &&  !aide)
	||
	(roomTab[roomNo].rtflags.PUBLIC == 0 && !loggedIn)
    )
	return ERROR;
    return roomNo;
}

/*
 * searchRooms()
 *
 * This function searches for user string in list of rooms.
 */
void searchRooms(char *target)
{
    int   i;

    mPrintf("Matching rooms:\n ");
    outFlag = OUTOK;

    for (i = 0; i < MAXROOMS;  i++) {
	if (legalMatch(i, target)) {
	    mPrintf(" %s ", formRoom(i, TRUE, TRUE));
	}
    }
}

/*
 * setUp()
 *
 * This does the initial setup based on who's logged on.
 */
void setUp(char justIn)
{
    int		g, i, j, ourSlot;
    extern long DoorsUsed;
    extern int  AnonMsgCount;
    extern int  IckyCount;

    if (justIn)   {
	echo = BOTH;	/* just in case	*/
	for (i = 0; i < UN_STACK; i++)
	    UngotoStack[i] = -1;

	heldMess = FALSE;
	IckyCount = 0;

	setmem(RoomMsgCount, MAXROOMS * sizeof *RoomMsgCount, 0);
    }

    if (!loggedIn)   {
	remoteSysop = FALSE;
	prevChar	= ' ';
	termWidth	= cfg.InitColumns;
	termLF		= TRUE;
	termNulls	= 5;
	expert		= FALSE;
	aide		= FALSE;
	sendTime	= TRUE;
	oldToo		= FALSE;
	HalfDup		= FALSE;
	FloorMode	= FALSE;
	DoorPriv	= FALSE;

	if (justIn)   {
	    logBuf.lbpage = pgdft;
	    /* set up logBuf so everything is new...	*/
	    AnonMsgCount = 0;

	    /* no mail for anonymous folks: */
	    roomTab[MAILROOM].rtlastMessage = cfg.newest;
	    for (i = 0; i < MAILSLOTS;  i++)
		logBuf.lbMail[i].rbmsgNo = 0l;

	    logBuf.lbname[0] = 0;

	    for (i = 0; i < MAXROOMS;  i++) {
		logBuf.lastvisit[i] = 0l;
		if (roomTab[i].rtflags.PUBLIC) {
		/* make public rooms known: */
		    logBuf.lbrgen[i] = roomTab[i].rtgen;
		} else {
		    /* make private rooms unknown: */
		    logBuf.lbrgen[i] = (roomTab[i].rtgen + (MAXGEN-1)) % MAXGEN;
		}
		lPtrTab[i]  = logBuf.lastvisit[i];
	    }
	}
    } else {
	/* loggedIn: */
	if (justIn)   {
	    DoorsUsed = 0l;
	    remoteSysop = FALSE;
	    /* set gen on all unknown rooms  --  INUSE or no: */
	    for (i = 0;  i < MAXROOMS;  i++) {
		j = abs(roomTab[i].rtgen - logBuf.lbrgen[i]);
		if (roomTab[i].rtflags.INUSE && !roomTab[i].rtflags.PUBLIC) {
		    /* it is private -- is it unknown? */
		    if (j != 0 && aide) {
			if (SomeSysop() || (cfg.BoolFlags.aideSeeAll &&
				(!roomTab[i].rtflags.INVITE || SomeSysop()))) {
			    SetKnown(0, i, &logBuf);
		        }
		    } else if ((j != 0 && j != RO_OFFSET) ||
			(!aide && i == AIDEROOM)
		    ) {
			/* yes -- set   gen = (realgen-1) % MAXGEN */
			/* j = (roomTab[i].rtgen + (MAXGEN-1)) % MAXGEN; */
			SetKnown(-2, i, &logBuf);
		    }
		}
		else if (logBuf.lbrgen[i] != roomTab[i].rtgen)  {
		    /* newly created public room -- remember to visit it; */
		    /* or room slot is not in use */
		    j = roomTab[i].rtgen - logBuf.lbrgen[i];
		    if (j < 0)
			g = -j;
		    else
			g = j;
		    if (g != FORGET_OFFSET && g != RO_OFFSET) {
			SetKnown(0, i, &logBuf);
		    }
		}
	    }

	    SetMailRoom();

#ifdef NEEDED
PagingOn();
#endif
	    for (i = 0;  i < MAXROOMS;  i++) {
		lPtrTab[i]  = logBuf.lastvisit[i];
#ifdef NEEDED
mPrintf("%3d. -> %ld\n ", i, lPtrTab[i]);
#endif
	    }
#ifdef NEEDED
PagingOff();
#endif

	    /* Slide entry to top of log table: */
	    ourSlot = logTab[thisSlot].ltlogSlot;
	    slideLTab(0, thisSlot);

	    logTab[0].ltpwhash      = hash(logBuf.lbpw);
	    logTab[0].ltnmhash      = hash(logBuf.lbname);
	    logTab[0].ltlogSlot     = ourSlot;
	    logTab[0].ltnewest      = cfg.newest;
	}
    }

    onConsole   = (whichIO == CONSOLE);
    if (thisRoom == MAILROOM)   fillMailRoom();
}

/*
 * SetMailRoom()
 *
 * This function is responsible for initializing the tables with the info
 * in the user's mail so Goto works out OK.
 */
void SetMailRoom()
{
    int i;
	    /* special kludge for Mail> room, to signal new mail:   */
    roomTab[MAILROOM].rtlastMessage = 0l;
    for (i = 0;  i < MAILSLOTS;  i++)
	if ((logBuf.lbMail[i].rbmsgNo & (~S_MSG_MASK)) &&
		    (logBuf.lbMail[i].rbmsgNo & S_MSG_MASK) > cfg.oldest)
	    roomTab[MAILROOM].rtlastMessage = S_MSG_MASK;

    if (roomTab[MAILROOM].rtlastMessage != S_MSG_MASK)
	roomTab[MAILROOM].rtlastMessage = 
			(logBuf.lbMail[MAILSLOTS-1].rbmsgNo & S_MSG_MASK);
}

/*
 * CheckForSkippedMsgs()
 *
 * This function does a check for skipped msgs (Mail).
 */
char CheckForSkippedMsgs()
{
    int i;

    for (i = 0;  i < ((thisRoom == MAILROOM) ? MAILSLOTS : MSGSPERRM);  i++)
	if ((roomBuf.msg[i].rbmsgNo & (~S_MSG_MASK)) &&
		(roomBuf.msg[i].rbmsgNo & S_MSG_MASK) >= cfg.oldest) {
	    roomTab[thisRoom].rtlastMessage = S_MSG_MASK;
	    return TRUE;
	}

    noteRoom();		/* just in case */
    return FALSE;
}

/*
 * systat()
 *
 * This function prints out current system status (.rs).
 */
void systat()
{
    extern char *VERSION;
    int		i;
    char	buffer[15];
    MSG_NUMBER  average, work;
    int		roomCount;

    for (roomCount = i = 0; i < MAXROOMS; i++)
	if (roomTab[i].rtflags.INUSE) roomCount++;

    mPrintf("This is %s\n ", &cfg.codeBuf[cfg.nodeTitle]);
    mPrintf("%s %s (V%s)\n",
			formDate(), Current_Time(), VERSION);

    if (loggedIn) {
	mPrintf(" Logged in as %s\n", logBuf.lbname);
	if (logBuf.lbflags.NET_PRIVS)
	    mPrintf(" You have net privileges, %d LD credits\n",
						logBuf.credit);
    }
    mPrintf(" %s messages,",    PrintPretty(cfg.newest-cfg.oldest+1, buffer));
    mPrintf(" last is %s\n",  PrintPretty(cfg.newest, buffer));
    mPrintf(" %dK message space,\n", cfg.maxMSector / (1024 / MSG_SECT_SIZE));
    mPrintf(" %d-entry log\n",		cfg.MAXLOGTAB		);
    mPrintf(" %d room slots, %d in use\n", MAXROOMS, roomCount);
    if (cfg.oldest > 1)
	work = cfg.maxMSector;
    else
	work = cfg.catSector;
    work *= MSG_SECT_SIZE;
    if (cfg.oldest > 1)
	average = (work) / (cfg.newest - cfg.oldest + 1);
    else
	average = (work) / (cfg.newest);
    mPrintf(" Average message length: %ld\n",  average);
}

/*
 * UngotoMaintain()
 *
 * This function maintains the Ungoto list.
 */
void UngotoMaintain(int lRoom)
{
	/* Move stack down 1 element */
    memmove(UngotoStack + 1, UngotoStack, (UN_STACK - 1) * sizeof(int));
	/* Add new element */
    UngotoStack[0] = lRoom;
}
