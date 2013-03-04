/*
 *				SysDoor.c
 *
 * Handles user-interface for doors within Citadel.  Note this is a system
 * dependent file, not necessarily portable.
 */

#define SYSTEM_DEPENDENT
#define INTERRUPTED_MESSAGES

#include "ctdl.h"
#include "c86door.h"

#define SYSTEM_DELAY	30

/*
 *				History
 *
 * 89Jan30  HAW AutoDoors.
 * 89Jan21  HAW Controlled access via #events
 * 88Dec16  HAW Created.
 */

char CheckDoorPriv(DoorData *DoorInfo, int Count, char NormalDoor);
#ifndef NO_DOORS

static SListBase DoorTime = { NULL, ChkTwoNumbers, NULL, free, EatTwoNumbers };
static char      *BpsStr;
       long      DoorsUsed = 0l;
static int       TheNewUserDoor = -1;
static int       TheAllUserDoor = -1;

extern MSG_NUMBER *lPtrTab;
extern int       UngotoStack[];
extern CONFIG    cfg;
extern MessageBuffer   msgBuf;
extern aRoom     roomBuf;
extern logBuffer logBuf;
extern long      *DL_Total;
extern UNS_32    BaudRate;
extern int       BaudFlags;
extern char      onConsole;
extern int       thisLog;
extern int       thisRoom;
extern char      ForceNet;
extern char      loggedIn;
extern char      remoteSysop;
extern char      CallSysop;
extern char      BpsSet;
extern int       SystemPort;
extern SListBase DL_List;
extern char *READ_ANY, *WRITE_ANY, outFlag;
extern int  exitValue;
extern char ExitToMsdos, *WRITE_ANY;
extern FILE *upfd;
extern long Door_Limit;

char RunDoorImmediate(int door);
/*
 * InitDoors()
 *
 * This function will initialize doors.
 */
void InitDoors()
{
    FILE	*fd;
    SYS_FILE    name;
    int		Count = 0;
    DoorData    DoorInfo;
    char	newuserfound = FALSE;
    char	alluserfound = FALSE;
    extern char *READ_ANY;

    makeSysName(name, DOOR_DATA, &cfg.roomArea);
    if ((fd = fopen(name, READ_ANY)) == NULL)
	return;

    while (!newuserfound && !alluserfound &&
			fread(&DoorInfo, sizeof DoorInfo, 1, fd) != 0) {
	if (!newuserfound && (DoorInfo.flags & DOOR_NEWUSER)) {
	    newuserfound = TRUE;
	    TheNewUserDoor = Count;
	}
	if (!alluserfound && (DoorInfo.flags & DOOR_ONLOGIN)) {
	    alluserfound = TRUE;
	    TheAllUserDoor = Count;
	}
	Count++;
    }

    fclose(fd);
}

/*
 * doDoor()
 *
 * This is the chief administrator for doors.
 */
char doDoor(char moreYet)
{
    char	DoorId[6], match = FALSE;
    int	Count = -1;
    FILE	*fd;
    SYS_FILE    name;
    DoorData    DoorInfo;
    long        NextPreempt;

    /* First handle simple privilege check */
    if (!DoorPriv) {
	mPrintf(" No privileges\n ");
	return GOOD_SELECT;
    }

    NextPreempt = TimeToNextPreemptive();

    if ((Door_Limit_On() && !HalfSysop() && DoorsUsed / 60 >= Door_Limit) ||
		(NextPreempt > 0l && NextPreempt < (3 * SYSTEM_DELAY))) {
	mPrintf("Sorry, no time for doors.\n ");
	return GOOD_SELECT;
    }

    /* see if there are any doors present. */
    makeSysName(name, DOOR_DATA, &cfg.roomArea);
    if ((fd = fopen(name, READ_ANY)) == NULL) {
	mPrintf("No doors appear to be available.\n ");
	return GOOD_SELECT;
    }

    /*
     * If moreYet, then wait for name.  If not, then print out current
     * doors and prompt for name.
     */
    if (!moreYet)
	if (!ShowDoors(fd)) {
	    mPrintf("There do not seem to be any doors available for you.\n ");
	    fclose(fd);
	    return GOOD_SELECT;
	}

    if (getNormStr((moreYet)?"":"door identifier", DoorId, 6,
		((moreYet) ? BS_VALID : 0) | QUEST_SPECIAL) == BACKED_OUT)
	return BACKED_OUT;

    if (DoorId[0] == '?') {
	ShowDoors(fd);
	fclose(fd);
	return GOOD_SELECT;
    }

    if (strLen(DoorId) != 0) {
	while (!match && fread(&DoorInfo, sizeof DoorInfo, 1, fd) != 0) {
	    match = (strCmpU(DoorId, DoorInfo.entrycode) == SAMESTRING &&
			(!DoorInfo.RoomName[0] || strCmpU(DoorInfo.RoomName,
			roomBuf.rbname) == SAMESTRING));
	    Count++;
	}
	if (!match || (DoorInfo.flags & DOOR_AUTO)
	           || (DoorInfo.flags & DOOR_ONLOGIN)
	           || (DoorInfo.flags & DOOR_NEWUSER)) {
	    mPrintf("? '%s' not found.\n ", DoorId);
	}
	else if (CheckDoorPriv(&DoorInfo, Count, TRUE))
	    RunAutoDoor(Count, TRUE);
    }
    fclose(fd);
    return GOOD_SELECT;
}

/*
 * CheckDoorPriv()
 *
 * True if this user can use door.
 */
char CheckDoorPriv(DoorData *DoorInfo, int Count, char NormalDoor)
{
    char Message(char *msg);

    if ((DoorInfo->flags & DOOR_AIDE) && !aide)
	return Message("Sorry, that door requires Aide privileges.\n ");
    else if ((DoorInfo->flags & DOOR_NEWUSER))
	return Message("Sorry, that door is for new users only.\n ");
    else if ((DoorInfo->flags & DOOR_SYSOP) && !HalfSysop())
	return Message("Sorry, that door requires SysOp privileges.\n ");
    else if (!(DoorInfo->flags & DOOR_MODEM) && !onConsole)
	return Message("Sorry, that door may not be run from remote.\n ");
    else if (!(DoorInfo->flags & DOOR_CON) && onConsole)
	return Message("Sorry, that door may not be run from sysConsole.\n ");
    else if (NormalDoor && NoTimeForDoor(Count, DoorInfo))
	return Message("Sorry, you may not use that door any longer.\n ");
    return TRUE;
}

/*
 * Message()
 *
 * This will write a message and return.
 */
static char Message(char *msg)
{
    mPrintf(msg);
    return FALSE;
}

/*
 * RunAutoDoor()
 *
 * This does the actual work of running a door.
 */
char RunAutoDoor(int i, char ask)
{
	Transition  Trans;
	SYS_FILE name;
	extern UNS_32 intrates[];

	if (i == -1) return FALSE;

	makeSysName(name, DOOR_DATA, &cfg.roomArea);

	Trans.DoorNumber = i;
	Trans.UserLog	= thisLog;
	Trans.RoomNum	= thisRoom;
	Trans.Port	= SystemPort;
	Trans.bps	= (int) (cfg.DepData.LockPort != -1 && BaudRate > 0l) ?
				intrates[cfg.DepData.LockPort] : BaudRate/10l;
	Trans.DCE	= (int) BaudRate;
	Trans.AnsiType  = (!ask) ? 0 : getYesNo("Do you have ANSI graphics");
	Trans.UserWidth = termWidth;
	Trans.mnp	= (BaudRate > 0l && (MNP & BaudFlags));

	strcpy(Trans.UserName, logBuf.lbname);
	strcpy(Trans.Sysop, cfg.SysopName);
	strcpy(Trans.System, cfg.codeBuf + cfg.nodeName);
	strcpy(Trans.DoorDir, name);
	Trans.Seconds = 0l;

	if (cfg.Audit != 0)
		makeAuditName(Trans.AuditLoc, "");
	else
		Trans.AuditLoc[0] = 0;

	Trans.TimeToNextEvent = TimeToNextPreemptive();
	if (Trans.TimeToNextEvent != -1l)
		Trans.TimeToNextEvent -= SYSTEM_DELAY;
	else
		Trans.TimeToNextEvent = 60 * 60;;

	if ((upfd = fopen("dorinfo2.def", WRITE_ANY)) == NULL) {
		mPrintf("C-86 internal error with doors.  Sorry.\n ");
		return FALSE;
	}
	else {
		fwrite(&Trans, sizeof Trans, 1, upfd);
		fclose(upfd);
		upfd = fopen("dorinfo3.def", WRITE_ANY);
		fwrite(lPtrTab, MAXROOMS * sizeof (MSG_NUMBER), 1, upfd);
		fwrite(UngotoStack, UN_STACK * sizeof (int), 1, upfd);
		InitEvTimes();
		fprintf(upfd, "%ld\n%d\n%d\n%d\n",
			BaudRate/10l, remoteSysop, CallSysop, ForceNet);
		RunList(&DoorTime, WrtTwoNumbers);
		fclose(upfd);
		if ((upfd = fopen("dorinfo4.def", "w")) != NULL) {
			RunList(&DL_List, WrtTwoNumbers);
			fclose(upfd);
		}
		exitValue = DOOR_EXIT;
		ExitToMsdos = TRUE;
		if (ask)
			mPrintf("Please be patient while the door is opened.\n ");
	}
	return TRUE;
}

/*
 * ShowDoors()
 *
 * This will show the doors available.
 */
static char ShowDoors(FILE *fd)
{
    DoorData DoorInfo;
    int      DoorCount = 0;

    while (fread(&DoorInfo, sizeof DoorInfo, 1, fd) != 0 && outFlag == OUTOK) {
	if (!(DoorInfo.flags & DOOR_AUTO) && !(DoorInfo.flags & DOOR_NEWUSER) &&
	    !(DoorInfo.flags & DOOR_ONLOGIN))
	    if (((DoorInfo.flags & DOOR_AIDE)  && aide) ||
	    ((DoorInfo.flags & DOOR_SYSOP) && HalfSysop()) ||
	    !(DoorInfo.flags & (DOOR_SYSOP | DOOR_AIDE)))
	    if (!DoorInfo.RoomName[0] ||
		strCmpU(DoorInfo.RoomName, roomBuf.rbname) == SAMESTRING) {
		DoorCount++;
		doCR();
		mPrintf("%-10s: %s", DoorInfo.entrycode, DoorInfo.description);
	    }
    }

    doCR();
    fseek(fd, 0l, 0);
    return (DoorCount != 0);
}

#ifdef NEEDED

/* strictly for cheating */
struct timeData {
    int   y, d, h, m;
    char  month[5];
    label person;
    char  newuser;
    char  evil;
    char  chat;
};

#endif


/*
 * BackFromDoor()
 *
 * This checks to see if we are returning from a door.  If so, then this should
 * restore the state of the installation to what it was when the door was run.
 */
char BackFromDoor()
{
    char	*ml;
    FILE	*fd;
    Transition  Trans;
    int	        ourBPS, h;
    long	*i;
    TwoNumbers  *temp;
    extern char ResolveDls, OnTime[];
    extern struct timeData lgin;

    if ((fd = fopen("dorinfo2.def", READ_ANY)) == NULL) {
	if (BpsSet) {
	    if (strCmpU(BpsStr, "LOCAL") == SAMESTRING ||
				strCmpU(BpsStr, "0") == SAMESTRING)
		BaudRate = 0l;
	    else SetUpPort(atoi(BpsStr));
	}
	return FALSE;
    }

    fread(&Trans, sizeof Trans, 1, fd);
    fclose(fd);
    unlink("dorinfo2.def");

    if ((fd = fopen("dorinfo3.def", READ_ANY)) != NULL) {
	fread(lPtrTab, MAXROOMS * sizeof (MSG_NUMBER), 1, fd);
	fread(UngotoStack, UN_STACK * sizeof (int), 1, fd);
	fgets(msgBuf.mbtext, 10, fd);
	ourBPS = atoi(msgBuf.mbtext);
	fgets(msgBuf.mbtext, 10, fd);
	remoteSysop = atoi(msgBuf.mbtext);
	fgets(msgBuf.mbtext, 10, fd);
	CallSysop = atoi(msgBuf.mbtext);
	fgets(msgBuf.mbtext, 10, fd);
	ForceNet = atoi(msgBuf.mbtext);
	MakeList(&DoorTime, "", fd);
	fclose(fd);
	unlink("dorinfo3.def");
	RunList(&DoorTime, Cumulate);
	/* InitEvents(FALSE); */
    }
    temp = (TwoNumbers *) GetDynamic(sizeof *temp);
    temp->first = Trans.DoorNumber;
    i = (long *) SearchList(&DoorTime, temp);
    if (i != NULL)
	temp->second = (*i) + Trans.Seconds;
    else temp->second = Trans.Seconds;
    AddData(&DoorTime, temp, NULL, TRUE);

    loggedIn = TRUE;
    ResolveDls = FALSE;
    MakeList(&DL_List, "dorinfo4.def", NULL);
    unlink("dorinfo4.def");
    getLog(&logBuf, Trans.UserLog);
    if ((DL_Total = SearchList(&DL_List, (temp = MakeTwo(thisLog, 0l))))
    						== NULL) {
	DL_Total = &temp->second;
	AddData(&DL_List, temp, NULL, TRUE);
    }
    else free(temp);
    getRoom(Trans.RoomNum);

    SetUpPort(ourBPS);
    setUp(FALSE);
    SetMailRoom();
    logMessage(DOOR_RETURN, BaudRate, 0);

    h = lgin.hour;
    civTime(&h, &ml);
    sprintf(OnTime, "%d:%02d %s", h, lgin.minute, ml);

    GetIntMessage();

    return TRUE;
}

/*
 * NewUserDoor()
 *
 * This runs a validation door.
 */
char NewUserDoor()
{
    return RunDoorImmediate(TheNewUserDoor);
}

/*
 * LoggedInDoor()
 *
 * This is responsible for running a door on login if specified.
 */
char LoggedInDoor(void)
{
    return RunDoorImmediate(TheAllUserDoor);
}

static char RunDoorImmediate(int door)
{
    FILE	*fd;
    SYS_FILE    name;
    DoorData    DoorInfo;
    char	dir[100], drive;

    if (door != -1) {
	makeSysName(name, DOOR_DATA, &cfg.roomArea);
	if ((fd = fopen(name, READ_ANY)) == NULL)
	    return TRUE;

	if (fseek(fd, sizeof DoorInfo * door, 0) != 0) {
	    fclose(fd);
	    return TRUE;
	}

	fread(&DoorInfo, sizeof DoorInfo, 1, fd);
	fclose(fd);

	if (!(DoorInfo.flags & DOOR_MODEM) && !onConsole)
	    return TRUE;
	else if (!(DoorInfo.flags & DOOR_CON) && onConsole)
	    return TRUE;

	strcpy(dir, DoorInfo.location);
	MSDOSparse(dir, &drive);
	if (!realSetSpace(toUpper(drive) - 'A', dir)) {
	    return TRUE;
	}
	MakeCmdLine(msgBuf.mbtext, DoorInfo.CommandLine, "",
						MAXTEXT);
	ModemShutdown(FALSE);
	CitSystem(TRUE, "%s", msgBuf.mbtext);
	ModemOpen(TRUE);
    }
    homeSpace();
    return (onConsole || gotCarrier());
}

/*
 * NoTimeForDoor()
 *
 * This will see if there is still time to run the door.
 */
static char NoTimeForDoor(int which, DoorData *DoorInfo)
{
    TwoNumbers search;
    int	*found;

    if (DoorInfo->TimeLimit == -1) return FALSE;

    search.first = which;
    if ((found = (int *) SearchList(&DoorTime, &search)) != NULL)
	return ((*found) / 60l >= (long) DoorInfo->TimeLimit);

    return FALSE;
}

/*
 * ClearDoorTimers()
 *
 * This will clear the door timers list.
 */
void ClearDoorTimers()
{
    KillList(&DoorTime);
}

/*
 * Cumulate()
 *
 * This will find out how much time has been used by current user.
 */
static void Cumulate(TwoNumbers *temp)
{
    DoorsUsed += temp->second;	/* cumulate it ... */
}

/*
 * DoorHelpListing()
 *
 * This will list the doors for the help system.
 */
void DoorHelpListing(char *target)
{
    DoorData DoorInfo;
    SYS_FILE name;
    int count = 0;
    FILE     *fd;

    /* see if there are any doors present. */
    makeSysName(name, DOOR_DATA, &cfg.roomArea);
    if ((fd = fopen(name, READ_ANY)) == NULL) {
	sprintf(target, "    There are no doors available.\n");
	return;
    }

    target[0] = 0;
    while (fread(&DoorInfo, sizeof DoorInfo, 1, fd) != 0 && outFlag == OUTOK) {
	if (!(DoorInfo.flags & DOOR_AUTO) && !(DoorInfo.flags & DOOR_NEWUSER))
	    if (((DoorInfo.flags & DOOR_AIDE)  && aide) ||
	    ((DoorInfo.flags & DOOR_SYSOP) && HalfSysop()) ||
	    !(DoorInfo.flags & (DOOR_SYSOP | DOOR_AIDE)))
	    if (!DoorInfo.RoomName[0] ||
	   	 strCmpU(DoorInfo.RoomName, roomBuf.rbname) == SAMESTRING) {
		count++;
		sprintf(lbyte(target), "   %-10s: %s\n", DoorInfo.entrycode, DoorInfo.description);
	    }
    }
    fclose(fd);
    if (count == 0)
	sprintf(target, "    There are no doors available.\n");
}

#else
char doDoor(moreYet, first)
char moreYet;   /* TRUE to accept more parameters (dot command)	*/
char first;
{
    mPrintf("Sorry, doors are not available in this version of Citadel-86.\n ");
    return GOOD_SELECT;
}

char BackFromDoor()
{
    return FALSE;
}

void ClearDoorTimers()
{
}

void DoorHelpListing(char *target)
{
}

#endif

/*
 * ReadBps()
 *
 * This will read the bps off the command line.
 */
void ReadBps(char *str)
{
    BpsStr = str + 4;
}

/*
 * SetUpPort()
 *
 * This will setup the port appropriately.
 */
int SetUpPort(int bps)
{
    int code;

    code = BaudCode(bps);
    if (bps > 0)
	BaudRate = ((UNS_32) bps) * (UNS_32) 10;
    else
	BaudRate = (UNS_32) bps;

    if (code == -1) {
	printf("door error, unrecognizable bps.\n");
	code = 0;
    }
    setNetCallBaud(code);

    return code;
}

/*
 * BaudCode()
 *
 * This will find out what our baud code is.
 */
int BaudCode(int bps)
{
    switch (bps) {
	case 0:   return cfg.sysBaud;
	case 30:  return 0;
	case 120: return 1;
	case 240: return 2;
	case 480: return 3;
	case 960: return 4;
	case 1440: return 5;
	case 1920: return 6;
	case 3840: return 7;
	case 5680: return 8;
	default: return -1;
    }
}
