/*
 *				netedit.c
 *
 * Sysop network editing functions.  Split out due to compiler limitations.
 */

/*
 *				history
 *
 * 94Sep02 HAW  Created.
 */
#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"

/*
 *				contents
 *
 */

static char	*SupportedBauds[] = {
	"300", "3/12", "3/24", "3/48", "3/96", "3/14.4", "3/19.2"
};

/*
 *		External variable definitions for NET.C
 */
char *BaudString =
	"Baud code (0=300, 1=1200, 2=2400, 3=4800, 4=9600, 5=14.4, 6=19.2)";
extern CONFIG    cfg;		/* Lots an lots of variables    */
extern rTable    *roomTab;
extern char      *confirm;
extern NetBuffer netTemp;
extern logBuffer logBuf;	/* Person buffer		*/
extern int       thisRoom;
extern NetBuffer netBuf;
extern char	 *ALL_LOCALS;
extern logBuffer logTmp;	/* Person buffer		*/
extern int	 PriorityMail;
extern FILE	 *upfd;
extern char      loggedIn;	/* Is we logged in?		*/
extern char      outFlag;	/* Output flag			*/
extern char      onConsole;
extern char      haveCarrier;	/* Do we still got carrier?     */
extern char      modStat;	/* Needed so we don't die       */
extern SListBase UntilNetSessions;
extern MenuId GetListId;
extern int       thisNet;
extern char	 *DomainFlags;
extern MessageBuffer   msgBuf;
extern NetTable  *netTab;

static void GetAllSharedRooms(void);

/*
 * netStuff()
 *
 * This function handles main net menu.
 */
void netStuff()
{
    extern char *who_str;
    extern char ForceNet;
    TwoNumbers  tmp;
    char	work[50];
    label       who;
    int		logNo;
    MenuId	id, id2;
    long	Redials, duration;
    char	*NetMiscOpts[] = {
	"Add node to netlist\n", "Credit setting\n", "Dial system",
	"Edit a node\n", "Initiate Anytime Net Session", "Local list\n",
	"Net privileges\n", "Priority Mail\n", "Request File\n",
	"Send File\n", "Until Done Net Sessions", "View net list\n", "X\beXit",
#ifdef ZNEEDED
"Z",
#endif
	""
    };
    int AdminPriorityMail(char *line, int arg);

    /* If we don't net, don't allow this. */
    if (!cfg.BoolFlags.netParticipant) {
	SysopInfoReport(NO_MENU, "Networking is disabled on this installation.\n ");
	return ;
    }

    id = RegisterSysopMenu("netopt.mnu", NetMiscOpts, " Net Menu ", 0);
    do {
	outFlag = OUTOK;
	RegisterThisMenu("netopt.mnu", NetMiscOpts);
	SysopMenuPrompt(id, "\n Net function: ");
	switch (GetSysopMenuChar(id)) {
	    case ERROR:
	    case 'X':
		CloseSysopMenu(id);
		return;
	    case 'P':
		getList(AdminPriorityMail, "Systems and Priority Mail",
							NAMESIZE, TRUE, 0);
		break;
	    case 'I':
		ForceAnytime();
		sprintf(work, "now %s.\n ", ForceNet ? "ON" : "OFF");
		SysopInfoReport(id, work);
		break;
	    case 'R':   /* File requests */
		SysopRequestString(id, "System", who, sizeof who, 0);
		if (!ReqNodeName("", who, NULL, RNN_SYSMENU, &netBuf))
		    break;
		fileRequest();
		break;
	    case 'S':   /* File transmissions */
		SysopRequestString(id, "System", who, sizeof who, 0);
		if (!ReqNodeName("", who, NULL, RNN_SYSMENU, &netBuf))
		    break;
		getSendFiles(id, who);
		break;
	    case 'C':   /* Set users' LD credits */
		if ((logNo = GetUser(who, &logTmp, TRUE)) == ERROR ||
				logNo == cfg.MAXLOGTAB) break;
		sprintf(work,
			"Currently %d credits.  How many now", logTmp.credit);
		logTmp.credit = (int) SysopGetNumber(id, work, 0l, 1000l);
		sprintf(work, "Set to %d.", logTmp.credit);
		SysopInfoReport(id, work);
		if (loggedIn  &&  strCmpU(logBuf.lbname, who) == SAMESTRING)
		    logBuf.credit = logTmp.credit;

		putLog(&logTmp, logNo);
		break;
	    case 'D':   /* Primitive dial out ability.  Don't get excited */
		if (!onConsole) break;
		if (gotCarrier()) {	/* carrier already?  just jump in */
		    CloseSysopMenu(id);
		    interact(FALSE);
		    id = RegisterSysopMenu("netopt.mnu", NetMiscOpts,
							" Net Menu ", 0);
		    break;
		}
		/* Get node to call, if none specified abort */
		SysopRequestString(id, "System", who, sizeof who, 0);
		if (!ReqNodeName("", who, NULL, RNN_ONCE | RNN_SYSMENU,&netBuf))
		    break;

		/* How many times should we try to call? */
		if ((Redials = SysopGetNumber(id, "# of redial attempts", 0l,
							65000l)) <= 0l)
		    Redials = 1l;       /* allow empty C/R to generate 1. */

		/* Modem should be disabled since we're in CONSOLE mode. */
		id2 = SysopContinual(netBuf.netName, "Dialing ...", 50, 13);
		EnableModem(FALSE);
		for (; Redials > 0l; Redials--) {
		    /* if successful call, start chattin'! */
		    if (makeCall(FALSE, id2)) {
			SysopCloseContinual(id2);
			CloseSysopMenu(id);
			mputChar(BELL);
			interact(FALSE);
			id = RegisterSysopMenu("netopt.mnu", NetMiscOpts,
							" Net Menu ", 0);
			break;
		    }
		    /* This handles an abort from kbd */
		    if (KBReady()) {
			getCh();
			/* Hope this turns off modem */
			outMod(' '); pause(2);
			DisableModem(FALSE);
			break;
		    }
		    /* printf("Failed\n"); */
		    /* Let modem stabilize for a moment. */
		    for (startTimer(WORK_TIMER);chkTimeSince(WORK_TIMER) < 3l; )
			;
		}
		SysopCloseContinual(id2);
		/*
		 * If we don't have carrier disable the modem.  We have this
		 * check in case sysop wants to perform download within
		 * Citadel.
		 */
		if (!gotCarrier()) {
		    DisableModem(FALSE);
		    modStat = haveCarrier = FALSE;
		}
		break;
	    case 'V':   /* View the net list. */
		CloseSysopMenu(id);
		doCR();
		writeNet(TRUE, FALSE);
		if (NeedSysopInpPrompt()) modIn();
		id = RegisterSysopMenu("netopt.mnu", NetMiscOpts,
							" Net Menu ", 0);
		break;
	    case 'L':
		CloseSysopMenu(id);
		writeNet(TRUE, TRUE);
		if (NeedSysopInpPrompt()) modIn();
		id = RegisterSysopMenu("netopt.mnu", NetMiscOpts,
							" Net Menu ", 0);
		break;
	    case 'A':   /* Add a new node to the list */
		addNetNode();
		break;
	    case 'E':   /* Edit a node that is on the list */
		SysopRequestString(id, "Name of system to edit", who, sizeof who,0);
		if (!ReqNodeName("", who, NULL, RNN_ONCE | RNN_DISPLAY |
							RNN_SYSMENU, &netBuf))
		    break;

		CloseSysopMenu(id);
		editNode();
		id = RegisterSysopMenu("netopt.mnu", NetMiscOpts,
							" Net Menu ", 0);
		break;
	    case 'N':   /* Give someone net privileges. */
		NetPrivs(who);
		break;
	    case 'U':
		id2 = SysopContinual(" Net Session ", "", 30, 4);
		SysopContinualString(id2, "Member Net", work, 3, 0);
		tmp.first = atoi(work);
		if (tmp.first < 1 || tmp.first > MAX_NET - 1) {
		    if (strlen(work) != 0)
			SysopError(id2, "Illegal Member Net");
		}
		else {
		    if (SearchList(&UntilNetSessions, &tmp) != NULL) {
			SysopInfoReport(NO_MENU, "Net session deactivated.\n");
			KillData(&UntilNetSessions, &tmp);
		    }
		    else {
			SysopContinualString(id2, "Duration", work, 4, 0);
			duration = atol(work);
			if (duration < 1l)
			    SysopError(id2, "Illegal Duration");
			else
			    AddData(&UntilNetSessions,MakeTwo(tmp.first,duration),
								NULL, FALSE);
		    }
		}
		SysopCloseContinual(id2);
		break;
#ifdef ZNEEDED
	    case 'Z':
		inNet = NORMAL_NET;
		AddNetMsgs("tempmail.$$$", inMail, FALSE, MAILROOM, TRUE);
		inNet = NON_NET;
		break;
#endif
	}
    } while (onLine());
}

/*
 * AdminPriorityMail()
 *
 * This handles the administration of priority mail.
 */
static int AdminPriorityMail(char *system, int arg)
{
    if (searchNameNet(system, &netBuf) == ERROR) {
	SysopError(NO_MENU, "No such system\n");
    }
    else {
	if (!(netBuf.nbflags.normal_mail ||
		netBuf.nbflags.HasRouted ||
		AnyRouted(thisNet) ||
		DomainFlags[thisNet])) {
	    SysopError(NO_MENU, "No outgoing mail.\n");
	    return TRUE;
	}
	else if ((netBuf.MemberNets & PRIORITY_MAIL)) {
	    SysopInfoReport(NO_MENU, "Priority mail deactivated.\n");
	    PriorityMail--;
	    netBuf.MemberNets &= ~(PRIORITY_MAIL);
	}
	else {
	    PriorityMail++;
	    netBuf.MemberNets |= (PRIORITY_MAIL);
	}
	putNet(thisNet, &netBuf);
    }
    return TRUE;
}

/*
 * NetPrivs()
 *
 * This will setup net privs for someone.
 */
void NetPrivs(label who)
{
    int logNo, result;
    char work[50];

    if ((logNo = GetUser(who, &logTmp, TRUE)) == ERROR) return;
    if (logNo == cfg.MAXLOGTAB) {
	result = DoAllQuestion("Give everyone net privs",
					"Take away everyone's net privs");
	if (result == ERROR) return;
	for (logNo = 0; logNo < cfg.MAXLOGTAB; logNo++) {
    	    getLog(&logTmp, logNo);
    	    if (!onConsole) mPrintf(".");
    	    if (logTmp.lbflags.L_INUSE && logTmp.lbflags.NET_PRIVS != result) {
    		logTmp.lbflags.NET_PRIVS = result;
    		putLog(&logTmp, logNo);
	    }
	}
	return;
    }
    sprintf(work, "%s has %snet privileges\n ", who,
				(logTmp.lbflags.NET_PRIVS) ? "no " : "");
    if (!SysopGetYesNo(NO_MENU, work, confirm))   return;
    logTmp.lbflags.NET_PRIVS = !logTmp.lbflags.NET_PRIVS;
    if (strCmpU(logTmp.lbname, logBuf.lbname) == SAMESTRING)
	logBuf.lbflags.NET_PRIVS = logTmp.lbflags.NET_PRIVS;

    putLog(&logTmp, logNo);
}

/*
 * getSendFiles()
 *
 * This will get the files from the sysop to send to another system.
 */
static void getSendFiles(MenuId id, label sysName)
{
    SYS_FILE       sysFile;
    char	   temp[10];
    extern char    *APPEND_ANY;

    sprintf(temp, "%d.sfl", thisNet);
    makeSysName(sysFile, temp, &cfg.netArea);
    if ((upfd = fopen(sysFile, APPEND_ANY)) == NULL) {
	SysopPrintf(id, "Couldn't open %s for update?\n ", sysFile);
	return ;
    }
    sprintf(msgBuf.mbtext, "Files to send to %s", sysName);
    if (getList(addSendFile, msgBuf.mbtext, 126, TRUE, 0)) {
	netBuf.nbflags.send_files = TRUE;
	putNet(thisNet, &netBuf);
    }
    fclose(upfd);
}

/*
 * addSendFile()
 *
 * This is a work function, called indirectly by getList().
 */
static int addSendFile(char *Files, int arg)
{
    struct fl_send sendWhat;

    if (sysGetSendFilesV2(GetListId, Files, &sendWhat)) {
	putSLNet(sendWhat, upfd);
	return TRUE;
    }

    return ERROR;
}

/*
 * addNetNode()
 *
 * This adds a node to the net listing.
 */
static void addNetNode()
{
    int searcher, gen;
    char  found;
    extern char *ALL_LOCALS;
    MenuId id;

    id = SysopContinual("", "", 2 * NAMESIZE, 5);
    for (searcher = 0; searcher < cfg.netSize; searcher++)
	if (netTab[searcher].ntflags.in_use == FALSE) break;

    if (searcher != cfg.netSize) {
	getNet(searcher, &netBuf);
	found = TRUE;
	gen = (netBuf.nbGen + 1) % NET_GEN;
    }
    else {
	found = FALSE;
	gen = 0;
    }

    killNetBuf(&netBuf);
    zero_struct(netBuf);	/* Useful initialization       */
    initNetBuf(&netBuf);

	/* Get a unique name */
    if (!GetSystemName(netBuf.netName, -1, id)) {
	SysopCloseContinual(id);
	return;
    }

	/* Get a unique ID */
#ifndef OLD_STYLE
    if (!GetSystemId(netBuf.netId, -1, id)) {
	SysopCloseContinual(id);
	return;
    }
#else
    do {
	goodAnswer = TRUE;
	SysopContinualString(id, "System ID", netBuf.netId, NAMESIZE, 0);
	if (strlen(netBuf.netId) == 0) {
	    SysopCloseContinual(id);
	    return;
	}
	if (searchNet(netBuf.netId, &netTemp) != ERROR) {
	    sprintf(msgBuf.mbtext, "Sorry, %s is already in use.\n ",
							netBuf.netId);
	    SysopError(id, msgBuf.mbtext);
	    goodAnswer = FALSE;
	}
    } while (!goodAnswer);
#endif

    netBuf.baudCode = (int) SysopGetNumber(id, BaudString, 0l, 6l);
    netBuf.nbflags.local	= SysopGetYesNo(id, "", "Is system local");
    netBuf.nbflags.in_use	= TRUE;
    netBuf.MemberNets		= 1;     /* Default */
    netBuf.nbGen		= gen;   /* Update generation #  */
    netBuf.nbflags.RouteTo	= TRUE;
    netBuf.nbflags.RouteFor	= TRUE;

    if (!found) {
	if (cfg.netSize != 0)
	    netTab = (NetTable *) 
			realloc(netTab, sizeof (*netTab) * ++cfg.netSize);
	else
	    netTab = (NetTable *) 
			GetDynamic(sizeof(*netTab) * ++cfg.netSize);
	searcher = cfg.netSize - 1;
    }
    putNet(searcher, &netBuf);
    InitVNode(searcher);
    DomainInit(FALSE);		/* so we can redirect easily enough */
    SysopCloseContinual(id);
}

/*
 * GetSystemName()
 *
 * Get a new system name.
 */
static int GetSystemName(char *buf, int curslot, MenuId id)
{
    char  goodAnswer;
    int slot;

    do {
	SysopContinualString(id, "System name", buf, NAMESIZE, 0);
	if (strlen(buf) == 0) {
	    return FALSE;
	}
	if ((goodAnswer = strCmpU(ALL_LOCALS, buf)) == 0)
	    SysopError(id, "Sorry, reserved name\n ");
	else if (strchr(buf, '_') != NULL) {
	    goodAnswer = FALSE;
	    SysopError(id, "Please don't use '_' in the system name.\n ");
	}
	else {
	    slot = searchNameNet(buf, &netTemp);
	    if (slot != ERROR && slot != curslot) {
		sprintf(msgBuf.mbtext, "Sorry, %s is already in use.\n ", buf);
		SysopError(id, msgBuf.mbtext);
		goodAnswer = FALSE;
	    }
	}
    } while (!goodAnswer);
    return TRUE;
}

/*
 * GetSystemId()
 *
 * This function gets a system id from the user and does error checking.
 */
static int GetSystemId(char *buf, int curslot, MenuId id)
{
    char goodAnswer;
    int  slot;

    do {
	goodAnswer = TRUE;
	SysopContinualString(id, "System ID", buf, NAMESIZE, 0);
	if (strlen(buf) == 0) {
	    return FALSE;
	}
	if ((slot = searchNet(buf, &netTemp)) != ERROR && slot != curslot) {
	    sprintf(msgBuf.mbtext, "Sorry, %s is already in use.\n ",
							buf);
	    SysopError(id, msgBuf.mbtext);
	    goodAnswer = FALSE;
	}
    } while (!goodAnswer);
    return TRUE;
}

/*
 * MemberNets()
 *
 * This adds nets to this system's list.
 */
static int MemberNets(char *netnum, int add)
{
    int num;
    MULTI_NET_DATA temp;

    num = atoi(netnum);
    if (num < 1 || num > MAX_NET - 1) {
	SysopError(NO_MENU, "There are only 31 nets to choose from.\n");
	return TRUE;
    }
    temp = 1l;
    temp <<= (num-1);
    if (add)
	netBuf.MemberNets |= temp;
    else {
	temp = ~temp;
	netBuf.MemberNets &= temp;
    }
    return TRUE;
}

/*
 * editNode()
 *
 * This function will edit a net node.
 */
static void editNode()
{
    label  temp2;
    char   title[50], work[80], temp[NAMESIZE*3];
    int    place, compress;
    MenuId id, id2;
    char   exttemp;
    char   *NetEditOpts[] = {
	"Access setting\n", "Baud code change\n", "Condensed name\n",
	"Download toggle", "External Dialer", "Fast Transfers\n",
	"ID change\n",
	"Kill node from list\n", "Local setting\n", "Member Nets\n",
	"Name change\n", "Othernet toggle\n", "Passwords\n", "Room stuff\n",
	"Spine settings\n", "Values", "Wait for room", "X\beXit",
#ifdef NEEDED
	"ZKludge",
#endif
	""
    };

    place = thisNet;    /* this is really a kludge, but for now will serve */

    if (!NeedSysopInpPrompt())	/* rather icky, really.  fix someday?	*/
	NodeValues(NO_MENU);

    sprintf(title, " Editing %s ", netBuf.netName);
    id = RegisterSysopMenu("netedit.mnu", NetEditOpts, title, 0);

    while (onLine()) {
	outFlag = OUTOK;
	sprintf(work, "\n (%s) edit fn: ", netBuf.netName);
	SysopMenuPrompt(id, work);
	switch (GetSysopMenuChar(id)) {
	    case ERROR:
	    case 'X':
		putNet(place, &netBuf);
		CloseSysopMenu(id);
		return;
	    case 'E':
		exttemp = netBuf.nbflags.ExternalDialer;
		if ((netBuf.nbflags.ExternalDialer =
		SysopGetYesNo(id, "", "Use an external dialer to reach this system"))) {
		    SysopRequestString(id, "Dialer information",
				netBuf.access, sizeof netBuf.access, 0);
		}
		else if (exttemp)	/* clear old information */
		    netBuf.access[0] = 0;
		break;
	    case 'A':
		SysopRequestString(id, "Access string", netBuf.access,
						sizeof netBuf.access, 0);
		break;
	    case 'B':
		netBuf.baudCode = (int) SysopGetNumber(id, BaudString, 0l, 6l);
		break;
	    case 'C':
		SysopRequestString(id, "shorthand for node", temp, 3, 0);
		if (strCmpU(temp, netBuf.nbShort) == 0)
		    break;
		if (searchNameNet(temp, &netTemp) != ERROR) {
		    sprintf(work, "'%s' is already in use.", temp);
		    SysopError(id, work);
		}
		else
		    strcpy(netBuf.nbShort, temp);
		break;
	    case 'D':
		sprintf(work, "for %s %s.\n ",
			netBuf.netName, netBuf.nbflags.NoDL ? "ON" : "OFF");
		SysopInfoReport(id, work);
		netBuf.nbflags.NoDL = !netBuf.nbflags.NoDL;
		break;
	    case 'F':
		netBuf.nbflags.MassTransfer = !netBuf.nbflags.MassTransfer;
		if (netBuf.nbflags.MassTransfer) {
			/* kludges - next major release make into char */
		    if ((compress = GetUserCompression()) == NO_COMP) {
			netBuf.nbflags.MassTransfer = FALSE;
			RegisterThisMenu("netedit.mnu", NetEditOpts);
			break;
		    }
		    else netBuf.nbCompress = compress;
		    RegisterThisMenu("netedit.mnu", NetEditOpts);
		}
		sprintf(work, "for %s %s.\n ", netBuf.netName,
				netBuf.nbflags.MassTransfer ? "ON" : "OFF");
		SysopInfoReport(id, work);
		if (netBuf.nbflags.MassTransfer) {
		    MakeNetCacheName(temp, thisNet);
		    mkdir(temp);
		}
		putNet(thisNet, &netBuf);
		/* more work here? */
		break;
	    case 'R':
		CloseSysopMenu(id);
		RoomStuff(title);
		id = RegisterSysopMenu("netedit.mnu", NetEditOpts, title, 0);
		break;
	    case 'N':
		id2 = SysopContinual("", "", 2 * NAMESIZE, 5);
		if (GetSystemName(temp, thisNet, id2))
		    strcpy(netBuf.netName, temp);
		SysopCloseContinual(id2);
		break;
	    case 'I':
		id2 = SysopContinual("", "", 2 * NAMESIZE, 5);
		if (GetSystemId(temp, thisNet, id2))
		    strcpy(netBuf.netId, temp);
		SysopCloseContinual(id2);
		break;
	    case 'K':
		if (netBuf.nbflags.normal_mail) {
		    sprintf(work, "There is outgoing mail outstanding.\n ");
		    SysopInfoReport(id, work);
		}
		if (netBuf.nbflags.room_files) {
		    sprintf(work, "There are file requests outstanding.\n ");
			SysopInfoReport(id, work);
		}
		if (SysopGetYesNo(id, "", "Confirm")) {
		    EachSharedRoom(thisNet, KillShared, KillShared, NULL);
		    UpdateSharedRooms();
		    netBuf.nbflags.in_use = FALSE;
		    putNet(place, &netBuf);
		    KillTempFiles(thisNet);
		    KillCacheFiles(thisNet);
		    CloseSysopMenu(id);
		    return;
		}
		break;
	    case 'L':
		netBuf.nbflags.local = SysopGetYesNo(id, "", "Is system local");
		break;
	    case 'P':
		CloseSysopMenu(id);
		mPrintf(" Current passwords\n");
		mPrintf(" Our password: %s\n", netBuf.OurPwd);
		mPrintf(" Their password: %s\n", netBuf.TheirPwd);
		if (getXString("our new password", temp2, NAMESIZE, "", ""))
		    strcpy(netBuf.OurPwd, temp2);
		if (getXString("their new password", temp2, NAMESIZE, "", ""))
		    strcpy(netBuf.TheirPwd, temp2);
		id = RegisterSysopMenu("netedit.mnu", NetEditOpts, title, 0);
		break;
	    case 'M':
		getList(MemberNets, "Nets to add to this system's member list",
								5, TRUE, TRUE);
		getList(MemberNets,"Nets to take off this system's member list",
								5, TRUE, FALSE);
		break;
	    case 'S':
		sprintf(msgBuf.mbtext, "We will be a spine for %s", 
								netBuf.netName);
		if (!(netBuf.nbflags.spine =
					SysopGetYesNo(id, "", msgBuf.mbtext))) {
		    sprintf(msgBuf.mbtext, "%s will be a spine",
							netBuf.netName);
		    netBuf.nbflags.is_spine =
					SysopGetYesNo(id, "", msgBuf.mbtext);
		}
		else
		    netBuf.nbflags.is_spine = FALSE;
		break;
	    case 'O':
		sprintf(work, " System is %sOtherNet\n ",
				(netBuf.nbflags.OtherNet) ? "not " : "");
		SysopInfoReport(id, work);
		netBuf.nbflags.OtherNet = !netBuf.nbflags.OtherNet;
		break;
	    case 'V':
		NodeValues(id);
		break;
	    case 'W':
		netBuf.nbflags.Login = !netBuf.nbflags.Login;
		sprintf(work, "Wait %s", netBuf.nbflags.Login ? "ON" : "OFF");
		SysopInfoReport(id, work);
		break;
#ifdef NEEDED
	    case 'Z':
		netBuf.nbHiRouteInd = (int) getNumber("Kludge value", 0l, 255l);
		netBuf.nbflags.HasRouted = TRUE;
		break;
#endif
	}
    }
}

/*
 * RoomStuff()
 *
 * This function handles the user when selecting 'r' on the net edit menu.
 */
static void RoomStuff(char *title)
{
	char   work[50];
	int    CurRoom;
	int AddSharedRooms(char *data, int arg);
	int DelSharedRooms(char *data, int arg);
	MenuId id;
	char *RoomOpts[] = {
		"Add Rooms\n", "Get All Shared Rooms\n", "Show Rooms\n",
		"Unshare Rooms", "X\beXit",
		"",
	};
	char done = FALSE;

	/*
	 * We use SHOW_MENU_OLD so the options are printed every last time
	 */
	id = RegisterSysopMenu("netrooms.mnu", RoomOpts, title, 0);
	CurRoom = thisRoom;
	while (onLine() && !done) {
		outFlag = OUTOK;
		sprintf(work, "\n (Rooms: %s) edit fn: ", netBuf.netName);
		SysopMenuPrompt(id, work);
		switch (GetSysopMenuChar(id)) {
		case ERROR:
		case 'X':
			done = TRUE;
			break;
		case 'A':
			getList(AddSharedRooms, "Rooms to Share as Backbone", NAMESIZE,
								TRUE, BACKBONE);
			getList(AddSharedRooms,"Rooms to Share as Peon",NAMESIZE,
								TRUE, PEON);
			break;
		case 'U':
			getList(DelSharedRooms,"Rooms to Unshare",NAMESIZE,TRUE, 0);
			break;
		case 'S':
			CloseSysopMenu(id);
			PagingOn();
			mPrintf("\n ");
			EachSharedRoom(thisNet, DumpRoom, DumpVRoom, NULL);
			if (onConsole) modIn();
			PagingOff();
			id = RegisterSysopMenu("netrooms.mnu", RoomOpts, title, 0);
			break;
		case 'G':
			CloseSysopMenu(id);
			GetAllSharedRooms();
			id = RegisterSysopMenu("netrooms.mnu", RoomOpts, title, 0);
		}
	}
	getRoom(CurRoom);	/* restore state */
	UpdateSharedRooms();
	CloseSysopMenu(id);
}

static int SelectGetAllOption(char *name);
/*
 * GetAllSharedRooms()
 *
 * This function is responsible for making the node currently being edited
 * share all rooms.
 */
static void GetAllSharedRooms()
{
	int rover, abort;
	RoomSearch arg;
	extern VirtualRoom *VRoomTab;
	extern int VirtSize;

	/* normal rooms */
	for (abort = FALSE, rover = 0; rover < MAXROOMS && !abort; rover++) {
		if (roomTab[rover].rtflags.INUSE &&
				roomTab[rover].rtflags.SHARED) {
			strcpy(arg.Room, roomTab[rover].rtname);
			if (!RoomRoutable(&arg)) {
				abort = !SelectGetAllOption(roomTab[rover].rtname);
			}
		}
	}

	/* virtual rooms */
	for (rover = 0; rover < VirtSize && !abort; rover++) {
		if (VRoomInuse(rover)) {
			strcpy(arg.Room, VRoomTab[rover].vrName);
			if (!RoomRoutable(&arg)) {
				abort = !SelectGetAllOption(VRoomTab[rover].vrName);
			}
		}
	}
}

/*
 * SelectGetAllOption()
 *
 * this is a worker bee function for selection of whether a room should be
 * shared during the traversal of all rooms.
 */
static int SelectGetAllOption(char *name)
{
	MenuId menu_id;
	char title_buffer[NAMESIZE + 15];
	char *GetAllOpts[] = {
		"Backbone\n", "Peon\n", "Don't Share\n", "Abort\n", ""
	};
	int retval;

	sprintf(title_buffer, " %s ", name);
	menu_id = RegisterSysopMenu("getall.mnu", GetAllOpts, title_buffer,
							SHOW_MENU_OLD);
	SysopMenuPrompt(menu_id, "\n Sharing function: ");

	switch (GetSysopMenuChar(menu_id)) {
	default:
	case 'D':
		retval = TRUE;
		break;
	case 'A':
		retval = FALSE;
		break;
	case 'P':
		AddSharedRooms(name, PEON);
		retval = TRUE;
		break;
	case 'B':
		AddSharedRooms(name, BACKBONE);
		retval = TRUE;
		break;
	}
	CloseSysopMenu(menu_id);
	return retval;
}

/*
 * AddSharedRooms()
 *
 * This function allows the addition of shared rooms from node editing.
 */
static int AddSharedRooms(char *data, int ShType)
{
    extern VirtualRoom *VRoomTab;
    int roomslot;
    RoomSearch arg;
    SharedRoomData *room;
    char virt = FALSE;

    strcpy(arg.Room, data);
    if (RoomRoutable(&arg)) {
	SetMode(arg.room->room->mode, ShType);
	arg.room->srd_flags |= SRD_DIRTY;
	return TRUE;
    }

    if ((roomslot = FindVirtualRoom(data)) == ERROR) {
	if ((roomslot = roomExists(data)) == ERROR) {
	    SysopPrintf(GetListId, "No such room.\n");
	    return TRUE;
	}
	else if (!roomTab[roomslot].rtflags.SHARED) {
	    SysopPrintf(GetListId, "Room is not shared.\n");
	    return TRUE;
	}
    }
    else virt = TRUE;

    room = NewSharedRoom();
    room->room->srslot   = roomslot;
    room->room->sr_flags = (virt) ? SR_VIRTUAL : 0;
    room->room->lastPeon = VRoomTab[roomslot].vrLoLocal;
    room->room->lastMess = (virt) ? VRoomTab[roomslot].vrLoLD : cfg.newest;
    room->room->srgen    = (virt) ? 0 : roomTab[roomslot].rtgen + (unsigned) 0x8000;
    room->room->netGen   = netBuf.nbGen;
    room->room->netSlot  = thisNet;
    SetMode(room->room->mode, ShType);

    return TRUE;
}

/*
 * DelSharedRooms()
 *
 * This function kills the specified room sharing link, if it exists.
 */
static int DelSharedRooms(char *data, int arg)
{
    RoomSearch search;

    strcpy(search.Room, data);
    if (!RoomRoutable(&search)) {
	SysopPrintf(GetListId, "Not found\n");
	return TRUE;
    }
    KillShared(search.room, thisNet, search.room->room->srslot, NULL);
    return TRUE;
}

/*
 * KillTempFiles()
 *
 * This eliminates unneeded temp files for dead node.
 */
static void KillTempFiles(int which)
{
    label    temp;
    SYS_FILE temp2;

    sprintf(temp, "%d.ml", which);
    makeSysName(temp2, temp, &cfg.netArea);
    unlink(temp2);
    netBuf.nbflags.normal_mail = FALSE;
    sprintf(temp, "%d.rfl", which);
    makeSysName(temp2, temp, &cfg.netArea);
    unlink(temp2);
    netBuf.nbflags.room_files = FALSE;
    sprintf(temp, "%d.sfl", which);
    makeSysName(temp2, temp, &cfg.netArea);
    unlink(temp2);
    netBuf.nbflags.send_files = FALSE;
    sprintf(temp, "%d.vtx", which);
    makeSysName(temp2, temp, &cfg.netArea);
    unlink(temp2);
    InitVNode(thisNet);
}

/*
 * NodeValues()
 *
 * This function prints out the values for the current node.
 */
static void NodeValues(MenuId id)
{
    int	i, first;
    MULTI_NET_DATA h;

    sprintf(msgBuf.mbtext, "\n Node #%d: %s", thisNet, netBuf.netName);
    if (strlen(netBuf.nbShort))
	sprintf(lbyte(msgBuf.mbtext), " (%s)", netBuf.nbShort);

    sprintf(lbyte(msgBuf.mbtext), "\n Id: %s (%slocal @ %s)\n ",
				netBuf.netId,
				netBuf.nbflags.local ? "" : "non",
				SupportedBauds[netBuf.baudCode]);

    if (netBuf.nbflags.ExternalDialer)
	sprintf(lbyte(msgBuf.mbtext), "External Dialer Information: %s\n ",
								netBuf.access);

    if (strlen(netBuf.access) != 0 && !netBuf.nbflags.ExternalDialer)
	sprintf(lbyte(msgBuf.mbtext), "Access: %s\n ", netBuf.access);

    if (netBuf.nbflags.spine)
	sprintf(lbyte(msgBuf.mbtext), "We are a spine\n ");
    else if (netBuf.nbflags.is_spine)
	sprintf(lbyte(msgBuf.mbtext), "This system is a spine\n ");

    if (netBuf.nbflags.OtherNet)
	sprintf(lbyte(msgBuf.mbtext), "OtherNet system.\n ");

    if (netBuf.nbflags.normal_mail || netBuf.nbflags.HasRouted)
	sprintf(lbyte(msgBuf.mbtext), "Outgoing Mail>.\n ");

    if (DomainFlags[thisNet])
	sprintf(lbyte(msgBuf.mbtext), "Outgoing DomainMail.\n ");

    if (AnyRouted(thisNet))
	sprintf(lbyte(msgBuf.mbtext), "Mail routed via this system.\n ");

    if (netBuf.nbflags.room_files)
	sprintf(lbyte(msgBuf.mbtext), "File requests outstanding.\n ");

    if (netBuf.nbflags.send_files)
	sprintf(lbyte(msgBuf.mbtext), "Files to be sent.\n ");

    if (netBuf.nbflags.MassTransfer)
	sprintf(lbyte(msgBuf.mbtext), "Fast Transfers on (using %s).\n ",
				GetCompEnglish(netBuf.nbCompress));

    if (netBuf.nbflags.Login)
	sprintf(lbyte(msgBuf.mbtext), "Wait for Room Prompt on call\n ");

    if (netBuf.MemberNets != 0l) {
	sprintf(lbyte(msgBuf.mbtext), "Assigned to net(s) ");
	for (i = 0, first = 1, h = 1l; i < MAX_NET; i++) {
	    if (h & netBuf.MemberNets) {
		if (!first)
		    sprintf(lbyte(msgBuf.mbtext), ", ");
		else first = FALSE;

		/* Yes - +1. Number the bits starting with 1 */
		sprintf(lbyte(msgBuf.mbtext), "%d", i+1);
	    }
	    h <<= 1;
	}
	sprintf(lbyte(msgBuf.mbtext), ".\n ");
    }
    else sprintf(lbyte(msgBuf.mbtext), "System is disabled.\n ");

    sprintf(lbyte(msgBuf.mbtext), "Last connected: %s\n",
					AbsToReadable(netBuf.nbLastConnect));
    SysopDisplayInfo(id, msgBuf.mbtext, " Values ");
}

/*
 * fileRequest()
 *
 * This handles the administration of requesting files from another system.
 */
static void fileRequest()
{
    struct fl_req file_data;
    label    data;
    char     loc[100], *c, *work;
    SYS_FILE fn;
    char     abort;
    FILE     *temp;
    int      place;
    extern char *APPEND_ANY;
    char     ambiguous, again;
    MenuId   id;

    place = thisNet;    /* again, a kludge to be killed later */

    id = SysopContinual("", "", 75, 10);
    sprintf(loc, "\nname of room on %s that has desired file", netBuf.netName);
    SysopContinualString(id, loc, file_data.room, NAMESIZE, 0);
    if (strlen(file_data.room) == 0) {
	SysopCloseContinual(id);
	return;
    }

    SysopContinualString(id, "\nthe file(s)'s name", loc, sizeof loc, 0);
    if (strlen(loc) == 0) {
	SysopCloseContinual(id);
	return;
    }

    ambiguous = !(strchr(loc, '*') == NULL && strchr(loc, '?') == NULL &&
		strchr(loc, ' ') == NULL);

    abort = !netGetAreaV2(id, loc, &file_data, ambiguous);

    if (!abort) {
	sprintf(data, "%d.rfl", place);
	makeSysName(fn, data, &cfg.netArea);
	if ((temp = fopen(fn, APPEND_ANY)) == NULL) {
	    SysopPrintf(id, "Couldn't append to '%s'????", fn);
	}
	else {
	    work = loc;

	    do {
		again = (c = strchr(work, ' ')) != NULL;
		if (again) *c = 0;
		strcpy(file_data.roomfile, work);
		if (ambiguous) strcpy(file_data.filename, work);
		fwrite(&file_data, sizeof (file_data), 1, temp);
		if (again) work = c + 1;
	    } while (again);

	    netBuf.nbflags.room_files = TRUE;
	    putNet(place, &netBuf);
	    fclose(temp);
	}
    }
    SysopCloseContinual(id);
}

