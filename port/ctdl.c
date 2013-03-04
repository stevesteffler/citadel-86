/*
 *				ctdl.c
 *
 * Command-interpreter code for Citadel.
 */

#define FAX_DEBUG

#define USER_INTERFACE
#define NET_INTERFACE
#define LOGIN

#include "ctdl.h"

/*
#define NEED_MSG_PEEKING 
#define NEED_MSG_SHOWING 
#define NEED_MSG_LIST 
*/

/*
 *				history
 *
 * 86Aug16 HAW  Kill history from file because of space problems.
 * 84May18 JLS/HAW Greeting modified for coherency.
 * 84Apr04 HAW  Upgrade to BDS 1.50a begun.
 * 83Mar08 CrT  Aide-special functions installed & tested...
 * 83Feb24 CrT/SB Menus rearranged.
 * 82Dec06 CrT  2.00 release.
 * 82Nov05 CrT  removed main() from room2.c and split into sub-fn()s
 */

/*
 *				Contents
 *
 *	doAide()		handles Aide-only       commands
 *	doChat()		handles C(hat)		command
 *	doEnter()		handles E(nter)		command
 *	doForget()		handles Z(Forget room)  command
 *	doGoto()		handles G(oto)		command
 *	doHelp()		handles H(elp)		command
 *	doKnown()		handles K(nown rooms)   command
 *	doLogin()		handles L(ogin)		command
 *	doLogout()		handles T(erminate)     command
 *	doMeet()		handles M(eet) User	command
 *	doRead()		handles R(ead)		command
 *	doRegular()		fanout for above commands
 *	doSkip()		handles S(kip)		command
 *	doSysop()		handles sysop-only      commands
 *	doUngoto()		handles U(ngoto)	command
 *	getCommand()		prints prompt and gets command char
 *	greeting()		System-entry blurb etc
 *	main()			has the central menu code
 */

char   ExitToMsdos = FALSE;     /* True when time to bring system down  */
int    exitValue = CRASH_EXIT;
static char NoChatAtAll = FALSE;
char   *confirm = "confirm";
char   *NoDownloads = "\n Sorry, this room does not allow downloads\n";
char   ConsolePassword;
char	*FaxString;

extern CONFIG     cfg;		/* The main variable to be saved      */
extern aRoom      roomBuf;	/* Room buffer		*/
extern SListBase  Moderators;
extern MessageBuffer    msgBuf;	/* Message buffer		*/
extern MessageBuffer    tempMess;	/* Message buffer		*/
extern logBuffer  logBuf;	/* Person's log buffer		*/
extern logBuffer  logTmp;	/* Person's log buffer		*/
extern rTable     *roomTab;	/* Room index for RAM		*/
extern LogTable   *logTab;	/* Log  index for RAM		*/
extern struct floor *FloorTab;
extern long       FDSectCount;	/* size of files in directory		*/
extern int	  thisRoom;	/* Current room		*/
extern SECTOR_ID  pulledMLoc;	/* Loc of msg to be pulled		*/
extern char *VERSION, *LCHeld, *netVersion, *SysVers;
extern MSG_NUMBER pulledMId;	/* Id of msg to be pulled		*/
extern char       *who_str;
extern char       remoteSysop;
extern char       onConsole;	/* Where IO is ...		*/
extern char       whichIO;	/* Where IO is ...		*/
extern char       outFlag;
extern char       loggedIn;	/* Are we logged in?		*/
extern char       echo;
extern char       newCarrier;	/* Just got carrier, hurrah!    */
extern char       justLostCarrier;/* Boo, hiss!		*/
extern char       textDownload;	/* flag		*/
extern char       haveCarrier;
extern char       *baseRoom;
extern char       heldMess;
extern char       anyEcho;
extern char       PrintBanner;
extern int	  CurLine;

extern int      acount;
#define AUDIT   9000
extern char     audit[AUDIT];
extern SListBase DirBase;

/*
 * doAide()
 *
 * This function handles the aide-only menu.
 *
 * return FALSE to fall invisibly into default error msg.
 */
char doAide(char moreYet, char first)
{
	label oldName;
	int  rm;
	char chatStack;
	char fname[100];
	char *ValAide[] = {
		"Chat\n", "Delete empty rooms\n", "Edit room\n",
		"Insert message\n", "Kill room\n", "S\bNot available.\n",
		"\b", " ", ""
	};
	extern char *APrivateRoom;

	if (roomBuf.rbflags.ISDIR == 1 && HalfSysop())
	ExtraOption(ValAide, "Add File\n");

	if (!aide) {
		PushBack('E');
	}

	if (moreYet)   first = '\0';

	if (first)	PushBack(first);

	RegisterThisMenu("aide.mnu", ValAide);

	switch (GetMenuChar()) {
	case 'A':
		getString("Full filename", fname, sizeof fname, 0);
		if (access(fname, 0) != 0) {
			mPrintf("No such file.\n ");
			break;
		}
		if (CopyFileGetComment(fname, thisRoom, msgBuf.mbtext)) {
			SetSpace(FindDirName(thisRoom));
			if (strlen(msgBuf.mbtext) > 0) {
				if (getYesNo("Use old comment")) {
					updFiletag(fname,
						strchr(msgBuf.mbtext, ' ')+1);
				}
				else FileCommentUpdate(fname, FALSE, TRUE);
			}
			else FileCommentUpdate(fname, FALSE, TRUE);
			homeSpace();
		}
		break;
	case '\b':
		mPrintf("\b \b");	/* not sure why this is necessary */
		return BACKED_OUT;
	case 'C':
		logMessage(SET_FLAG, 0l, LOG_CHATTED);
		if (NoChatAtAll && !SomeSysop()) {
			if (!printHelp("nochat", HELP_USE_BANNERS))
			printHelp("nochat.blb",
				HELP_NO_LINKAGE|HELP_BITCH|HELP_SHORT);
		}
		else {
			chatStack = cfg.BoolFlags.noChat;
			cfg.BoolFlags.noChat = FALSE;
			if (whichIO == MODEM)	ringSysop();
			else			interact(TRUE) ;
			cfg.BoolFlags.noChat = chatStack;
		}
		break;
	case 'D':
		ZeroMsgBuffer(&msgBuf);
		sprintf(msgBuf.mbtext, "The following empty rooms deleted by %s: ",
								logBuf.lbname);
		if (!getYesNo(confirm))
			break;
		strcpy(oldName, roomBuf.rbname);
		indexRooms();

		if ((rm=roomExists(oldName)) != ERROR)  getRoom(rm);
		else					getRoom(LOBBY);

		aideMessage(NULL, /* noteDeletedMessage== */ FALSE );
		break;
	case 'E':
		renameRoom();
		break;
	case 'I':
		ZeroMsgBuffer(&msgBuf);
		if (
			thisRoom   == AIDEROOM
			||
			pulledMId  == 0l
		)   {
			mPrintf("No message to insert.");
			break;
		}
		if (!getYesNo(confirm))
			break;
		noteAMessage(roomBuf.msg, MSGSPERRM, pulledMId, pulledMLoc);
		putRoom(thisRoom);
		noteRoom();
		sprintf(
			msgBuf.mbtext,
			"Following message inserted in %s> by %s",
			formRoom(thisRoom, FALSE, FALSE), logBuf.lbname);
		aideMessage(NULL, /* noteDeletedMessage == */ TRUE);
		break;
	case 'K':
		if (
			thisRoom == LOBBY
			||
			thisRoom == MAILROOM
			||
			thisRoom == AIDEROOM
		) {
			mPrintf(" not here!");
			break;
		}
		if (!getYesNo(confirm))   break;

		ZeroMsgBuffer(&msgBuf);
		sprintf(
			msgBuf.mbtext,
			"%s> killed by %s",
			roomBuf.rbname,
			logBuf.lbname
		);
		aideMessage(NULL, /* noteDeletedMessage == */ FALSE);
	
		KillInfo(roomBuf.rbname);
		KillSharedRoom(thisRoom);
		KillData(&DirBase, NtoStrInit(thisRoom, "", 0, TRUE));
		WriteAList(&DirBase, "ctdldir.sys", WrtNtoStr);
		roomBuf.rbflags.INUSE = FALSE;
		putRoom(thisRoom);
		noteRoom();
		getRoom(LOBBY);
		break;
	case 'S':
#ifdef NEED_AVAILABLE
		changeDate();
#endif
		break;
	}
	return GOOD_SELECT;
}

/*
 * doChat()
 * 
 * Chatting!
 */
char doChat(char moreYet, char first)
{
	extern SListBase ChatOn;

	if (moreYet)   first = '\0';

	if (first)     oChar(first);

	if (whichIO != MODEM) {
		interact(TRUE) ;
		if (whichIO == CONSOLE)
			if (getYesNo("Back to MODEM mode")) {
				whichIO = MODEM;
				setUp(FALSE);
				if (!gotCarrier()) EnableModem(FALSE);
			}
	}
	else {
		logMessage(SET_FLAG, 0l, LOG_CHATTED);

		if (!IsChatOn()) {
			if (!printHelp("nochat", HELP_USE_BANNERS))
					printHelp("nochat.blb",
					HELP_NO_LINKAGE|HELP_BITCH|HELP_SHORT);
	    			return GOOD_SELECT;
		}

		ringSysop();
	}
	return GOOD_SELECT;
}

/*
 * doEnter()
 *
 * This function handles the E(nter) command.
 */
char doEnter(char moreYet, char first)
{
#define CONFIGURATION   0
#define MESSAGE		1
#define PASSWORD	2
#define ROOM		3
#define ENTERFILE       4
#define CONTINUED       5
#define NETWORK		6
#define DEFAULT_MESSAGE	7
#define OR_UPLOAD	8
	OfflineReader *Reader;
    char what;			/* one of above seven */
    SListBase  ESelects = { NULL, FindSelect, NULL, NoFree, NULL };
    char *EnterOpts[] = {
	TERM "\r", TERM "\n", NTERM "Xmodem", NTERM "Ymodem",
#ifdef WXMODEM_AVAILABLE
	NTERM "Wxmodem",
#endif
	TERM "Configuration", TERM "Message", TERM "Password",
	TERM "Room", TERM "Held Message", TERM "Net-Message",
	/* These are for external protocols -- don't delete them! */
	" ", " ", " ",
	" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", ""
    };
    char abort, Protocol, Prot, again;
    char *letter, cmdbuf[30];

    if (loggedIn && roomBuf.rbflags.ISDIR == 1)
	ExtraOption(EnterOpts, TERM "F");

    AddExternProtocolOptions(EnterOpts, TRUE);
    if (loggedIn) AddOfflineReaderOptions(EnterOpts, TRUE);

    if (moreYet)   first = '\0';

    abort       = FALSE  ;

    if (thisRoom != MAILROOM && !loggedIn &&
					!cfg.BoolFlags.unlogEnterOk) {
	mPrintf("Must log in to enter messages except MAIL to the SYSOP\n ");
	return GOOD_SELECT;
    }

    if (first)     PushBack(first);

    do {
	again = FALSE;
	outFlag = OUTOK;
	Protocol	= ASCII  ;
	what	= DEFAULT_MESSAGE;
	if (CmdMenuList(EnterOpts, &ESelects, "entopt.mnu", cmdbuf,
						moreYet, TRUE) == BACKED_OUT)
	    return BACKED_OUT;

	letter = cmdbuf;
	do  {
	    switch (*letter) {
	    case '\r':
	    case '\n':
		break;
#ifdef WXMODEM_AVAILABLE
	    case 'X':
	    case 'Y':
	    case 'W':
		Protocol = (*letter == 'Y') ? YMDM : (*letter == 'X') ? XMDM : WXMDM;
		break;
#else
	    case 'X':
	    case 'Y':
		Protocol = (*letter == 'Y') ? YMDM : XMDM;
		break;
#endif
	    case 'F':
		if (Protocol == ASCII) {
		    mPrintf("\b\bXmodem F ");
		    Protocol = XMDM;
		}
		mPrintf("\bile upload ");
		if (!roomBuf.rbflags.UPLOAD || logBuf.lbflags.TWIT) {
		    mPrintf("\n Sorry, no uploads allowed.\n ");
		    abort = TRUE;
		}
		what    = ENTERFILE;
		break;
	    case 'C':
		again	= reconfigure();
		what	= CONFIGURATION;
		break;
	    case 'M':
		what	= MESSAGE;
		break;
	    case 'P':
		what	= PASSWORD     ;
		break;
	    case 'R':
		if (!cfg.BoolFlags.nonAideRoomOk && !aide)   {
		    mPrintf(" ?-- must be aide to create room\n ");
		    abort   = TRUE;
		    break;
		}
		if (!loggedIn) {
		    mPrintf("!Must log in to create a new room\n ");
		    abort = TRUE;
		    break;
		}
		if (logBuf.lbflags.TWIT) {
		    mPrintf("Sorry, no room in system.\n ");
		    abort = TRUE;
		    break;
		}
		what	= ROOM;
		break;
	    case 'H':
		what = CONTINUED;
		Protocol   = ASCII;	/* can't do this using protocol */
		break;
	    case 'N':
		what = NETWORK;
		break;
	    default:
		if ((Prot = FindProtocolCode(*letter, TRUE)) == -1) {
			if ((Reader = FindUpOR((*letter))) == NULL)
				abort = TRUE;
			else
				what = OR_UPLOAD;
		}
		else Protocol = Prot;
	}
	++letter;
	} while (moreYet && !abort && *letter);
    } while (again);

    KillList(&ESelects);

    doCR();

    if (!abort) {
	if (whichIO != CONSOLE && (loggedIn || ConsolePassword) &&
	    (thisRoom == MAILROOM || roomTab[thisRoom].rtflags.ANON))
	    echo = CALLER;
	switch (what) {
	case DEFAULT_MESSAGE:
		if (Protocol != ASCII) {
			mPrintf(" Message transfer"); doCR();
		}
	case MESSAGE	:   makeMessage(Protocol);	break;
	case PASSWORD	:   newPW()		;	break;
	case ROOM	:   makeRoom()		;	break;
	case ENTERFILE	:   upLoad(Protocol)	;	break;
	case CONTINUED	:   hldMessage(FALSE)	;	break;
	case NETWORK	:   netMessage(Protocol);	break;
	case OR_UPLOAD	:   OR_Upload(Reader, Protocol);	break;
	}
	echo = BOTH;
    }
    return GOOD_SELECT ;
}

/*
 * doForget()
 *
 * This function handles the (Forget room) command.
 */
char doForget(char expand)
{
    if (!expand) {
	mPrintf("%s\n ", roomBuf.rbname);
	if (thisRoom == LOBBY    ||
	    thisRoom == MAILROOM ||
	    thisRoom == AIDEROOM) {
	    mPrintf("!Can't forget this room! \n ");
	    return GOOD_SELECT ;
	}
	if (roomBuf.rbflags.UNFORGETTABLE) {
	    mPrintf("Room is unforgettable.\n ");
	    return GOOD_SELECT;
	}
	if (!getYesNo(confirm))   return GOOD_SELECT;
	SetKnown(FORGET_OFFSET, thisRoom, &logBuf);
	gotoRoom(baseRoom, MOVE_SKIP | MOVE_TALK);
    }
    else {
	/* mPrintf("\b\b "); */
	listRooms(FORGOTTEN);
    }
    return GOOD_SELECT;
}

/*
 * doGoto()
 *
 * This function handles the G(oto) command.
 */
char doGoto(char expand)
{
    label roomName;
    int   oldRoom;

    outFlag = IMPERVIOUS;

    if (!expand) {
	oldRoom = thisRoom;
	gotoRoom("", MOVE_GOTO | MOVE_TALK);
	if (oldRoom == thisRoom && loggedIn && !expert)
	    mPrintf("\n There are no more rooms with unread messages.\n ");
	return GOOD_SELECT;
    }

    if (getNormStr("", roomName, NAMESIZE, BS_VALID) == BACKED_OUT) {
	return BACKED_OUT;
    }

    if (roomName[0] == '?') {
	listRooms(NOT_INTRO);
    }
    else
	gotoRoom(roomName, MOVE_GOTO | MOVE_TALK);
    return GOOD_SELECT;
}

/*
 * doHelp()
 *
 * This function the handles the H(elp) command.
 */
char doHelp(char expand)
{
    label fileName;

    if (!expand) {
	mPrintf("\n\n");
	printHelp("mainhelp.hlp", HELP_BITCH);
	return GOOD_SELECT;
    }

    if (getNormStr("", fileName, (sizeof fileName) - 4, BS_VALID) == BACKED_OUT)
	return BACKED_OUT;

    if (strlen(fileName) == 0)
	strcpy(fileName, "mainhelp");

    if (fileName[0] == '?')     printHelp("helpopt.hlp", HELP_BITCH);
    else {
	/* adding the extention makes things look simpler for		*/
	/* the user... and restricts the files which can be read	*/
	strcat(fileName, ".hlp");
	printHelp(fileName, HELP_BITCH);
    }
    return GOOD_SELECT;
}

/*
 * doKnown()
 *
 * This function handles the K(nown rooms) command.
 */
char doKnown(char expand)
{
    char select = ERROR, c[2], again;
    label matchstr;
    char *KMenuOpts[] = {
	TERM "Anonymous rooms\n", TERM "Match", TERM "Directory rooms\n",
	TERM "Shared rooms\n", TERM "Private rooms\n",
	TERM "Z\bForgotten rooms\n", TERM "Read-only\n",
	TERM "\r", TERM "\n", " ", ""
    };
    SListBase  KSelects = { NULL, FindSelect, NULL, NoFree, NULL };

    if (!expand) {
	mPrintf("\n ");
	listRooms(NOT_INTRO);
    }
    else {
	if (!cfg.BoolFlags.NoInfo) ExtraOption(KMenuOpts, TERM "Information");
	do {
	    again = FALSE;
	    if (CmdMenuList(KMenuOpts, &KSelects, "known.mnu", c, TRUE, TRUE)
								== BACKED_OUT)
		return BACKED_OUT;

	    switch (c[0]) {
	    case 'I':
		AllInfo();
		break;
	    case 'A':
		select = ANON_SEL;
		break;
	    case 'M':
		if (getNormStr("",matchstr,NAMESIZE, BS_VALID) == BACKED_OUT) {
		    again = TRUE;
		    select = ERROR;
		    PushBack('\b');
		    oChar(' ');
		}
		else select = MATCH_SEL;
		break;
	    case 'D':
		select = DR_SEL;
		break;
	    case 'S':
		select = SH_SEL;
		break;
	    case 'P':
		select = PR_SEL;
		break;
	    case 'Z':
		select = FORGOTTEN;
		break;
	    case 'R':
		select = READONLY;
		break;
	    case '\r':
		doCR();
	    case '\n':
		strcpy(matchstr, "");
		select = MATCH_SEL;
		break;
	    }

	    if (select != MATCH_SEL && select != ERROR)
		listRooms(select);
	    else if (select != ERROR)
		searchRooms(matchstr);
	} while (again);
    }
    KillList(&KSelects);
    return GOOD_SELECT;
}

/*
 * doLogin()
 *
 * This function handles the L(ogin) command.
 */
char doLogin(char moreYet)
{
    label passWord;

    if (!moreYet)   mPrintf("\n");
    if (loggedIn)   {
	mPrintf("\n ?Already logged in!\n ");
	return GOOD_SELECT;
    }
    echo	= CALLER;
    if (getNormStr(moreYet ? "" : " password (just carriage return if new)",
			passWord, NAMESIZE, (moreYet) ? BS_VALID : NO_ECHO) ==
							BACKED_OUT) {
	return BACKED_OUT;
    }

    echo	= BOTH;
    login(passWord);
    return GOOD_SELECT;
}

#define LOGOUT_OPTS	\
"\n Logout options:\n \n Quit-also\n Stay\n Abort\n "

/*
 * doLogout()
 *
 * This function handles the T(erminate) command.
 */
char doLogout(char expand, char first)
{

    if (expand)   first = '\0';

    outFlag = IMPERVIOUS;

    if (heldMess && !cfg.BoolFlags.HoldOnLost) {
     mPrintf("\n WARNING: You have a message in your Hold Message Buffer!\n ");
	mAbort();	/* clear any first-run input fromuser */
    }

    if (first)   oChar(first);

    switch (toUpper(    first ? first : iChar()    )) {
    case '\b':
	if (expand) return BACKED_OUT;
    default:
	mPrintf(LOGOUT_OPTS);
	break;
    case 'Q':
	mPrintf("uit-also\n ");
	if (!expand)   {
	    if (!getYesNo(confirm))   break;
	}
	if (!onLine()) break;
	terminate( /* hangUp == */ TRUE, TRUE);
	break;
    case 'S':
	mPrintf("tay\n ");
	terminate( /* hangUp == */ FALSE, TRUE);
	break;
    case 'A':
	mPrintf("bort\n ");
	terminate( /* hangUp == */ TRUE, FALSE);
    }
    outFlag = OUTOK;
    return GOOD_SELECT;
}

OptValues Opt;

char revOrder;  /* Udderly HIDEOUS kludge.  MOOOOOO! */
char PhraseUser;

/*
 * doMeet()
 *
 * This function handles the M(eet) User command.
 */
char doMeet(char moreYet)
{
    label User;
    int   logNo;

    if (!moreYet) doCR();

    if (getNormStr(moreYet ? "" : "User to meet",
		User, NAMESIZE, QUEST_SPECIAL | BS_VALID) == BACKED_OUT) {
	return BACKED_OUT;
    }

    if (strlen(User) != 0) {
	if (User[0] == '?') {
	    BioDirectory();
	    return GOOD_SELECT;
	}

	/* this if catches "sysop" */
	if ((logNo = PersonExists(User)) == cfg.MAXLOGTAB)
	    logNo = findPerson(cfg.SysopName, &logTmp);

	if (logNo == ERROR) {
	    mPrintf("No such person\n ");
	}
	else {
	    getLog(&logTmp, logNo);
	    if (GetBioInfo(logNo)) {
	        mPrintf("\n The biography of %s\n ", logTmp.lbname);
	        doCR();
	        mPrintf("%s\n ", msgBuf.mbtext);
	    }
	    else {
	        mPrintf("%s doesn't have a biography.\n ", logTmp.lbname);
	    }
	}
    }

    return GOOD_SELECT;
}

char ManualMsgPage;

/* useful bit flags */
#define ReadFlagSet(x) 	(bit_flags & x)

#define COMPRESSED_BF	0x001
#define EXTDIR_BF	0x002
#define DODIR_BF	0x004
#define AGAIN_BF	0x008
#define HOSTF_BF	0x010
#define STATUS_BF	0x020
#define USER_BF		0x040
#define PHRASE_BF	0x080
#define GLOBAL_BF	0x100
#define FS_BF		0x200
#define ABORT_BF	0x400

/*
 * doRead()
 *
 * This function handles the R(ead) command.
 */
char doRead(char moreYet, char first)
{
	OfflineReader *Reader = NULL;
	UNS_16        bit_flags;
	char          Compressed,
		      whichMess,
		      protocol, prot,
		      ReadArchive;
	char          *letter, secondletter;
	char          fileName[100];
	int           CurRoom;
	SListBase  RSelects = { NULL, FindSelect, NULL, NoFree, NULL };
	SListBase  CSelects = { NULL, FindSelect, NULL, NoFree, NULL };
	extern char journalMessage, FormatFlag;
	extern int outPut;
	char *ReadOpts[] = {
		TERM "\r", TERM "\n",
		TERM "Forward", NTERM "Global", NTERM "Local-only", TERM "New",
		TERM "Old-reverse", TERM "Reverse", TERM "Status\n",
		NTERM "Xmodem", NTERM "More",
#ifdef WXMODEM_AVAILABLE
		NTERM "Wxmodem",
#endif
		NTERM "Ymodem", NTERM "User", NTERM "Phrase",
		/* these two are here rather than optional due to .RGE/.RGD */
		TERM "Directory", TERM "Extended-directory", " ", " ", " ", " ",
		" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
		" ", " ", " ", " ", " ", ""
	};
	char *CompOpts[] = {
		TERM "Directory(s)\n", TERM "File(s)\n", TERM "T\bFile(s)\n",
		TERM "B\bFile(s)\n", TERM "\nDirectory(s)", ""
	};
	char  cmdbuf[40];
	void *FindUser();
	int   UserOptAdd(char *str, int arg);

	whichMess = NEWoNLY; 
	if (moreYet)   first = '\0';

	zero_struct(Opt);
	Opt.Date = -1l;
	Opt.MaxMessagesToShow = -1;
	InitListValues(&Opt.Users, FindUser, NULL, free, NULL);

	if (thisRoom == MAILROOM && !loggedIn  &&
				!cfg.BoolFlags.unlogReadOk)   {
		showMessages(PAGEABLE|whichMess|revOrder,
				logBuf.lastvisit[thisRoom], OptionValidate);
		return GOOD_SELECT;
	}

	if (!loggedIn  &&  !cfg.BoolFlags.unlogReadOk)   {
		mPrintf("Must log in to read\n ");
		return GOOD_SELECT;
	}

	if (first)     PushBack(first);

	if (roomBuf.rbflags.ISDIR == 1 && loggedIn && !logBuf.lbflags.TWIT) {
		ExtraOption(ReadOpts, TERM "Binary file(s)");
		ExtraOption(ReadOpts, TERM "Textfile(s)");
		ExtraOption(ReadOpts, TERM "Archive");
	}

	if (AnyCompression())
		ExtraOption(ReadOpts, NTERM "Compressed");

	AddExternProtocolOptions(ReadOpts, FALSE);
	if (loggedIn) AddOfflineReaderOptions(ReadOpts, FALSE);

	if (HalfSysop())
		ExtraOption(ReadOpts, TERM "Invited-users");

    do {
	bit_flags = 0;
	protocol  = ASCII;
	whichMess = NEWoNLY; 
	ManualMsgPage = ReadArchive = PhraseUser = revOrder = FALSE;
	if (CmdMenuList(ReadOpts, &RSelects, "readopt.mnu", cmdbuf, moreYet, TRUE) ==
							BACKED_OUT)
	    return BACKED_OUT;

	letter = cmdbuf;
	do {
	    outFlag = OUTOK;

	    switch (*letter) {
	    case '\r':
		doCR();
	    case '\n':
		break;
	    case 'M':
		ManualMsgPage = TRUE;
		break;
	    case 'C':
	        bit_flags |= COMPRESSED_BF;
		break;
	    case 'A':
		if (logBuf.lbflags.DL_PRIVS || aide) {
		    if (roomBuf.rbflags.DOWNLOAD || HalfSysop()) {
			if (CmdMenuList(CompOpts, &CSelects, "", cmdbuf, TRUE,
						FALSE) == BACKED_OUT) {
			    bit_flags |= AGAIN_BF;
			    PushBack('\b');
			}
			else {
			    switch (cmdbuf[0]) {
			    case '\n':
			    case '\r':
			    case 'D':
				ReadArchive = 1;
				break;
			    case 'T':
			    case 'B':
			    case 'F':
				ReadArchive = 2;
				break;
			    default:
				bit_flags |= ABORT_BF;
			    }
			    break;
			}
		    }
		    else {
			mPrintf(NoDownloads);
			bit_flags |= ABORT_BF;
			break;
		    }
		}
		else bit_flags |= ABORT_BF;
	    break;
	    case 'F':
		revOrder    = FALSE;
		whichMess   = OLDaNDnEW;
		goto commondate;
	    case 'G':
		bit_flags |= GLOBAL_BF;
		break;
	    case 'L':
		Opt.LocalOnly = TRUE;
		break;
	    case 'N':
		whichMess   = NEWoNLY;
		goto commondate;
	    case 'O':
		revOrder    = REV;
		whichMess   = OLDoNLY;
		goto commondate;
	    case 'R':
		revOrder    = REV;
		whichMess   = OLDaNDnEW;
commondate:
		if (moreYet) {
		    if (getString("", fileName, NAMESIZE,
				BS_VALID | QUEST_SPECIAL) == BACKED_OUT) {
			bit_flags |= AGAIN_BF;
			oChar(' ');
			PushBack('\b');
			break;
		    }
		    if (fileName[0] == '?') {
			printHelp("readdate.blb", HELP_NO_LINKAGE | HELP_BITCH|HELP_SHORT);
			KillList(&RSelects);
			return GOOD_SELECT;
		    }

		    if (strlen(fileName) != 0)
			if (ReadMessageSpec(fileName, &Opt) == ERROR) {
			 mPrintf("\n Your message spec is unintelligible.\n ");
			    KillList(&RSelects);
			    return GOOD_SELECT;
			}
		}
		else {
		    doCR();
		}
		break;
	    case 'S':
		bit_flags |= STATUS_BF;
		break;
#ifdef WXMODEM_AVAILABLE
	    case 'X':
	    case 'W':
	    case 'Y':
		protocol    = (*letter == 'W') ? WXMDM :
					(*letter == 'X') ? XMDM : YMDM;
		break;
#else
	    case 'X':
	    case 'Y':
		protocol    = (*letter == 'X') ? XMDM : YMDM;
		break;
#endif
	    case 'B':
	    case 'T':
		bit_flags |= FS_BF;
	    case 'E':
	    case 'D':
		if (!logBuf.lbflags.DL_PRIVS && !aide) {
		    bit_flags |= ABORT_BF;
		    mPrintf("Sorry, you need privileges.\n ");
		    break;
		}
		if (ReadFlagSet(FS_BF)) {
		    if (roomBuf.rbflags.DOWNLOAD == 1 || TheSysop() ||
							remoteSysop) {
			bit_flags |= HOSTF_BF;
			textDownload    = (*letter == 'T') ? TRUE : FALSE;
			if (textDownload && protocol == ASCII) {
			    switch (secondletter = toUpper(modIn())) {
			    case '\b':
				bit_flags |= AGAIN_BF;
				PushBack('\b');
				break;
			    case 'F':
				mPrintf("Formatted");
				FormatFlag = TRUE;
				break;
			    default:
				PushBack(secondletter);
			    case '\r':
			    case '\n':
			    case ' ':
				doCR();
			    }
			}
		    }
		    else {
			mPrintf(NoDownloads);
			bit_flags |= ABORT_BF;
		    }
		}
		else {
		    if (roomBuf.rbflags.ISDIR == 1 || ReadFlagSet(GLOBAL_BF))   {
			if (ReadFlagSet(GLOBAL_BF) || roomBuf.rbflags.DOWNLOAD || TheSysop()
							|| remoteSysop) {
			    if (getNormStr("", fileName, sizeof fileName,
						BS_VALID) == BACKED_OUT) {
				PushBack('\b');
				bit_flags |= AGAIN_BF;
				oChar(' ');
				break;
			    }
			    if (*letter == 'D') bit_flags |= DODIR_BF; 
			    else bit_flags |= EXTDIR_BF;
			    break;
			}
			else {
			    mPrintf(NoDownloads);
			    bit_flags |= ABORT_BF;
			    break;
			}
		    }
		    else {
			mPrintf("\n This is not a directory room.\n ");
			bit_flags |= ABORT_BF;
		    }
		}
		break;
	    case 'I':
		if (doInviteDisplay() == BACKED_OUT) {
		    PushBack('\b');
		    bit_flags |= AGAIN_BF;
		    oChar(' ');
		    break;
		}
		KillList(&RSelects);
		return  GOOD_SELECT;
	    case 'P':
	 	bit_flags |= PHRASE_BF;
		PhraseUser = TRUE;
		break;
	    case 'U':
	 	bit_flags |= USER_BF;
		PhraseUser = TRUE;
		break;
	    default:
		if ((prot = FindProtocolCode(*letter, FALSE)) == -1) {
			if ((Reader = FindDownOR((*letter))) == NULL)
				bit_flags |= ABORT_BF;
		}
		else protocol = prot;
	    }
	    letter++;
	} while (moreYet && !ReadFlagSet(ABORT_BF) && *letter);
    } while (ReadFlagSet(AGAIN_BF));

    KillList(&RSelects);
    KillList(&CSelects);

    if (ReadFlagSet(ABORT_BF)) return GOOD_SELECT;

    if (ReadFlagSet(STATUS_BF)) {
	systat();
	return GOOD_SELECT;
    }

    if (ReadArchive) {
	if (ReadArchive == 1) {
	    getNormStr("ARChive filename(s)", fileName, sizeof fileName, 0);
	    if (ReadFlagSet(PHRASE_BF))
		getString("search phrase", Opt.Phrase, PHRASE_SIZE, 0);
	    if (strchr(fileName, '.') == NULL) strcat(fileName, ".arc");
	    wildCard(CompressedDir, fileName, Opt.Phrase, WC_MOVE | WC_DEFAULT);
	}
	else if (ReadArchive == 2)
	    SendArcFiles(protocol);
	return GOOD_SELECT;
    }

    if (ReadFlagSet(DODIR_BF) || ReadFlagSet(EXTDIR_BF)) {

	if (!Pageable) {
	    PagingOn();
	}

	if (ReadFlagSet(PHRASE_BF))
	    getString("search phrase", Opt.Phrase, PHRASE_SIZE, 0);

	if (!ReadFlagSet(GLOBAL_BF))
	    doDirectory(ReadFlagSet(DODIR_BF), fileName, Opt.Phrase);
	else {
	    CurRoom = thisRoom;
		/* should we have tableRunner() do this for us? */
	    for (thisRoom = 0; outFlag == OUTOK && thisRoom < MAXROOMS;
								thisRoom++)
		if (roomTab[thisRoom].rtflags.INUSE &&
			roomTab[thisRoom].rtflags.ISDIR &&
			(roomTab[thisRoom].rtflags.DOWNLOAD || SomeSysop()) &&
			knowRoom(&logBuf, thisRoom) == KNOW_ROOM) {
		    getRoom(thisRoom);
		    mPrintf("\n (%s)\n ", roomBuf.rbname);
		    doCR();		/* nice left side now */
		    doDirectory(ReadFlagSet(DODIR_BF), fileName, Opt.Phrase);
		    if (outFlag == OUTNEXT) outFlag = OUTOK;
		}
	    getRoom(CurRoom);
	}

	if (journalMessage) {
	    if (redirect(NULL, APPEND_TO)) {
		doDirectory(ReadFlagSet(DODIR_BF), fileName, Opt.Phrase);
		undirect();
	    }
	    journalMessage = FALSE;
	}
	PagingOff();
	return GOOD_SELECT;
    }

    if (ReadFlagSet(HOSTF_BF)) {
	if (ReadFlagSet(PHRASE_BF))
	    getString("search phrase", Opt.Phrase, PHRASE_SIZE, 0);
	TranFiles(protocol, Opt.Phrase);
	FormatFlag = FALSE;
	return GOOD_SELECT;
    }

    if (ReadFlagSet(USER_BF)) {
	getList(UserOptAdd, "Users", NAMESIZE * 3, FALSE, 0);
    }

    if (ReadFlagSet(PHRASE_BF))
	getString("search phrase", Opt.Phrase, PHRASE_SIZE, 0);

    if (ReadFlagSet(COMPRESSED_BF) || Reader != NULL) {
	if ((Compressed = GetUserCompression()) == NO_COMP)
	    return GOOD_SELECT;
	doCR();
    }
    else Compressed = NO_COMP;

    if (Reader != NULL && protocol == ASCII)
	protocol = XMDM;

    download(whichMess | revOrder,
             protocol,
	     ReadFlagSet(GLOBAL_BF) ? TRUE : FALSE,
	     Compressed,
	     Reader);

    KillList(&Opt.Users);
    Opt.Date = -1l;
    Opt.MaxMessagesToShow = -1;
    ManualMsgPage = FALSE;
    return GOOD_SELECT;
}

/*
 * UserOptAdd()
 *
 * This adds the given name to a list.
 */
static int UserOptAdd(char *str, int arg)
{
    AddData(&Opt.Users, strdup(str), NULL, FALSE);
    return TRUE;
}

/*
 * FindUser()
 *
 * Is the current user @system _ domain going to match?
 *
 * 7/26/94 - Also make this work against the CC list.
 */
static void *FindUser(char *element, int x)
/* x is actually not used -- we use global msgBuf */
{
	char Full[NAMESIZE + MB_AUTH + 2];
	void *FindUserWork();

	if (strlen(msgBuf.mboname) != 0) {
		sprintf(Full, "%s@%s", msgBuf.mbauth, msgBuf.mboname);
		if (strlen(msgBuf.mbdomain) != 0)
			sprintf(lbyte(Full), "_%s", msgBuf.mbdomain);
	}
	else
		strcpy(Full, msgBuf.mbauth);

	if (FindUserWork(Full, element) != NULL)
		return element;

	if (strlen(msgBuf.mbaddr) != 0) {
		sprintf(Full, "%s@%s", msgBuf.mbto, msgBuf.mbaddr);
	}
	else
		strcpy(Full, msgBuf.mbto);

	if (FindUserWork(Full, element) != NULL)
		return element;

	if (AltSearchList(&msgBuf.mbCC, FindUserWork, element) != NULL)
		return element;

	return NULL;
}

/* 
 * FindUserWork()
 *
 * Worker bee function.  Does the actual comparison of a username@system_dm
 * against some other string for a match.  Returns NULL on failure, otherwise
 * the found name.
 */
void *FindUserWork(char *name, char *target)
{
	label User;
	label nUser;
	char System[(2 * NAMESIZE) + 2];
	char nSystem[(2 * NAMESIZE) + 2];

	nSystem[0] = 0;
	SepNameSystem(name, nUser, nSystem, NULL);
	System[0] = 0;
	SepNameSystem(target, User, System, NULL);
	if (strlen(User) != 0 && matchString(nUser, User,
					lbyte(nUser)) == NULL) {
		return NULL;
	}
	if (strlen(System) != 0) {
	if (matchString(nSystem, System, lbyte(nSystem)) == NULL /* &&
			matchString(nDomain, Domain, lbyte(nDomain)) == NULL */)
		return NULL;
	}

	return name;
}

/*
 * OptionValidate()
 *
 * This is sent to showMessages.
 */
char OptionValidate(int mode, int slot)
{
    if (OptionCheck(mode, slot)) {
	printMessage(0, 0);
	return TRUE;
    }
    else mAbort();			/* give a chance to interrupt */
    return (mode == 1) ? TRUE : FALSE;
}

/*
 * OptionCheck()
 *
 * This function checks to see if all options fulfilled.
 */
char OptionCheck(char mode, int slot)
{
	long MsgTime;
	int  rover;

	if (mode == 1)
		return (!Opt.LocalOnly && GetFirst(&Opt.Users) == NULL && 
			strlen(Opt.Phrase) == 0 && Opt.Date == -1l);
	/* else */
	/*
	* If any match fails, don't print.  printMessage(0) indicates a
	* a print with msg still on disk, while a (1) indicates message now in
	* the message buffer.
	*/
	if (Opt.MaxMessagesToShow != -1)
	{
        	if ((!revOrder && slot < Opt.StartSlot) ||
			(revOrder && slot > Opt.StartSlot))
			return FALSE;
	}

	if (Opt.LocalOnly && msgBuf.mboname[0] && 
	    strCmpU(msgBuf.mboname, cfg.codeBuf + cfg.nodeName) != SAMESTRING)
		return FALSE;

	if (Opt.Date != -1l)
		if (ReadDate(msgBuf.mbdate, &MsgTime) != ERROR)
			if ((!revOrder && MsgTime < Opt.Date) ||
				(revOrder && MsgTime > Opt.Date))
				return FALSE;

	if (GetFirst(&Opt.Users) != NULL)
		if (SearchList(&Opt.Users, 0) == NULL) return NULL;

	if (strlen(Opt.Phrase) != 0) {
		getMsgStr(getMsgChar, msgBuf.mbtext, MAXTEXT);

		/* Kill extraneous line breaks */
		for (rover = 0; msgBuf.mbtext[rover]; rover++)
			if (msgBuf.mbtext[rover] == NEWLINE &&
				   msgBuf.mbtext[rover + 1] != ' ' &&
				   msgBuf.mbtext[rover + 1] != NEWLINE)
				msgBuf.mbtext[rover] = ' ';

		if (matchString(msgBuf.mbtext, Opt.Phrase, lbyte(msgBuf.mbtext))
								!= NULL) {
			findMessage(msgBuf.mbheadSector,atol(msgBuf.mbId),TRUE);
			return TRUE;
		}
		else return FALSE;
	}
	return TRUE;
}

/*
 * doDirectory()
 *
 * This function handles the read directory commands.
 */
void doDirectory(char doDir, char *fileName, char *phrase)
{
	int	    FileCount;
	extern long FDSize;
	extern char Showing;

	FDSize = 0l;
		/* 119.675 */
	Showing = DIR_LISTING;

	FileCount = wildCard((doDir) ? fDir : ShowVerbose, fileName, phrase,
							WC_DEFAULT | WC_MOVE);
	mPrintf("\n %d files, ", FileCount);
	mPrintf((doDir) ?	"approximately %s sectors total\n " :
				"%s bytes total.\n ",
				PrintPretty(FDSize, msgBuf.mbtext));
	GiveSpaceLeft(thisRoom);

		/* 119.675 */
	Showing = WHATEVER;
}

#define MAX_USER_ERRORS 25
/*
 * doRegular()
 *
 * The big fanout.
 */
char doRegular(char x, char c)
{
    static int errorCount = 0;
    char       toReturn, cc[2];
    SListBase  RegSelects = { NULL, FindSelect, NULL, NoFree, NULL };
    char *RegOpts[] = {
	TERM "Chat", TERM "Door", TERM "Enter", TERM "Goto",
	TERM "F\bread", TERM "Read", TERM "O\bread", TERM "N\bread",
	TERM "Help", TERM "Known rooms", TERM "Login",
	TERM "Skip", TERM "Terminate", TERM "Ungoto", TERM "\\", TERM ";",
	TERM "Z\bForget", TERM "?",
	" ", " ", " ", ""
    };
#ifdef NO_DOORS
    char       *legal = "CEFGHIKLMNORSTUZ";
#else
    char       *legal = "CDEFGHIKLMNORSTUZ";
#endif

    toReturn = GOOD_SELECT;

    if (!cfg.BoolFlags.NoInfo) ExtraOption(RegOpts, TERM "Information");
    if (loggedIn) {
	if (aide ||
		(strCmpU(logBuf.lbname, AskForNSMap(&Moderators, thisRoom))
								== SAMESTRING ||
		strCmpU(logBuf.lbname, FloorTab[thisFloor].FlModerator)
								== SAMESTRING))
	    ExtraOption(RegOpts, TERM "Aide special fn:");

	if (!cfg.BoolFlags.NoMeet) {
	    ExtraOption(RegOpts, TERM "Meet User");
	}
	else ExtraOption(RegOpts, TERM "Moo!");
    }

    if (strchr(legal, c) != NULL) errorCount = 0;
    else			  errorCount++;

    PushBack(c);	/* ugly kludge */
    if ((cc[0] = c) == 0 ||
		CmdMenuList(RegOpts, &RegSelects, NULL, cc, x, FALSE) == GOOD_SELECT) {
	switch (cc[0]) {
	case 'C': toReturn = doChat(  x, '\0');			break;
#ifndef NO_DOORS
	case 'D': toReturn = doDoor(  x);			break;
#endif
	case 'E': toReturn = doEnter( x, 'M' );			break;
	case 'F': toReturn = doRead(  x, 'F' );			break;
	case 'G': toReturn = doGoto(  x);			break;
	case 'H': toReturn = doHelp(  x);			break;
	case 'I': toReturn = doInfo();				break;
	case 'K': toReturn = doKnown( x);			break;
	case 'L': toReturn = doLogin( x);			break;
	case 'M': if (!cfg.BoolFlags.NoMeet) toReturn = doMeet( x);	break;
	case 'N': toReturn = doRead(  x, 'N' );			break;
	case 'O': toReturn = doRead(  x, 'O' );			break;
	case 'R': toReturn = doRead(  x, 'R' );			break;
	case 'S': toReturn = doSkip(  x);			break;
	case 'T': toReturn = doLogout(x, 'Q' );			break;
	case 'U': toReturn = doUngoto(x);			break;
	case '\'':
	case ';': toReturn = DoFloors();			break;
	case 0:
	    if (newCarrier)   {
		greeting();
		newCarrier  = FALSE;
	    }
	    if (justLostCarrier) {
		justLostCarrier = FALSE;
		terminate(TRUE, TRUE);
	    }
	    break;  /* irrelevant value */
	case '?':
	    printHelp("mainopt.mnu", HELP_BITCH|HELP_SHORT);
	    if (whichIO == CONSOLE)   mPrintf("\n^l: SysOp privileged fns\n ");
	    break;
	case 'A': toReturn = doAide(x, 'E');			break;
	case 'Z': toReturn = doForget(x);			break;
	default:
	    if (errorCount > MAX_USER_ERRORS) {
		logMessage(SET_FLAG, 0l, LOG_EVIL);
		HangUp(TRUE);
	    }
	    toReturn=BAD_SELECT;
	    break;
	}
    }
    if (toReturn == BACKED_OUT) {
	PushBack('\b');
	CmdMenuList(RegOpts, &RegSelects, NULL, cc, FALSE, FALSE);   /* does the BS */
    }
    KillList(&RegSelects);
    return  toReturn;
}

/*
 * doSkip()
 *
 * This function handles the <S>kip a room command.
 */
char doSkip(char expand)
{
    label roomName;			/* In case of ".Skip" */
    char  dispbuf[2 * NAMESIZE];
    int   rover;

    outFlag = IMPERVIOUS;
    sprintf(dispbuf, "%s> goto ", roomTab[thisRoom].rtname);
    mPrintf("%s", dispbuf);
    if (expand) {
	if (getNormStr("", roomName, NAMESIZE, BS_VALID) == BACKED_OUT) {
	    for (rover = 0; rover < strlen(dispbuf); rover++) {
		mPrintf("\b \b");
	    }
	    return BACKED_OUT;
	}
    }
    else
	roomName[0] = '\0';
    if (roomName[0] == '?')
	printHelp("skip.hlp", HELP_NO_LINKAGE | HELP_BITCH|HELP_SHORT);
    else {
	roomTab[thisRoom].rtflags.SKIP = 1;     /* Set bit */
	gotoRoom(roomName, MOVE_SKIP | MOVE_TALK);
    }
    return GOOD_SELECT;
}

/*
 * doSysop()
 *
 * This function handles the sysop-only menu.  It returns FALSE to fall
 * invisibly into default error msg.
 */
char doSysop()
{
    extern char *strFile, *NoFileStr;
    MSG_NUMBER  temp;
    char	systemPW[200];
    extern int  fixVers, majorVers;
    logBuffer   lBuf;		/* This has to be local!  Don't sub logTmp */
    MenuId	id;
    char	*CtdlOpts[] = {
	"Abort\n",
#ifdef CHANGE_BAUDS
	"Baud rate",
#endif
	"Chat mode", "Debug switch", "Echo",
	"File grab\n", "Information\n", "MODEM mode\n",
	"Net Menu", "Other Commands\n", "Reinitialize Modem\n",
	"Q (debug)", "User Admin", "X\beXit from " VARIANT_NAME,
#ifdef EVENT_DEBUG
"W",
#endif
#ifdef NEED_MSG_PEEKING
"Z",
#endif
#ifdef NEED_MSG_SHOWING
"9",
#endif
#ifdef NEED_MSG_LIST
"Y\n",
#endif
#ifdef NEEDED
"1",
#endif
#ifdef PEEK_LOG
"2",
#endif
#ifdef NEEDED
"3",
#endif
	""
    };
#ifdef NEED_MSG_LIST || PEEK_LOG
    int i;
#endif

    if ((!onConsole || ConsolePassword) && !remoteSysop) {
	if (!(onConsole && ConsolePassword && strlen(cfg.sysPassword) == 0)) {
	    if ((!aide && !onConsole) || strlen(cfg.sysPassword) == 0) {
		return BAD_SELECT;
	    }
	    echo	= CALLER;
	    getNormStr("password", systemPW, sizeof systemPW, NO_ECHO);
	    echo	= BOTH;
	    if (strcmp(systemPW, cfg.sysPassword) != 0)
		return BAD_SELECT;
	    remoteSysop = TRUE;
	}
    }

    initLogBuf(&lBuf);

    if (whichIO == CONSOLE && gotCarrier()) mPrintf("[One Moment]\n ");

    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts, " Privileged Functions ",0);
    while (onLine()) {

	outFlag = OUTOK;
	SysopMenuPrompt(id, "\n privileged fn: ");

	switch (GetSysopMenuChar(id)) {
#ifdef PEEK_LOG
	case '2':
	    CloseSysopMenu(id);
	    for (i = 0; i < MAXROOMS; i++) {
		printf("%ld\n", logBuf.lastvisit[i]);
	    }
	    iChar();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts,
					" Privileged Functions ", 0);
	    break;
#endif
#ifdef EVENT_DEBUG
	case 'W':
	    CloseSysopMenu(id);
	    EventShow();	/* Debug stuff */
	    iChar();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts,
					" Privileged Functions ", 0);
	    break;
#endif
#ifdef NEEDED
These must be modified for the windows interface before next use!
	case '1':
	    for (i = 0; i < MSGSPERRM; i++) {
		if (findMessage(roomBuf.msg[i].rbmsgLoc, roomBuf.msg[i].rbmsgNo,
								TRUE)) {
			mPrintf("(%s : %s) ", msgBuf.mbsrcId, msgBuf.mboname);
		}
		mPrintf("%ld: %d\n ", roomBuf.msg[i].rbmsgNo,
					roomBuf.msg[i].rbmsgLoc);
	    }
	    break;
#endif
#ifdef NEED_MSG_LIST
	case 'Y':
	    CloseSysopMenu(id);
	    for (i = 0; i < MSGSPERRM; i++)
		mPrintf("%ld: %d\n ", roomBuf.msg[i].rbmsgNo,
					roomBuf.msg[i].rbmsgLoc);
	    iChar();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts,
					" Privileged Functions ", 0);
	    break;
#endif
#ifdef NEED_MSG_PEEKING
	case 'Z':
	    CloseSysopMenu(id);
	    mPeek();
	    iChar();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts,
					" Privileged Functions ", 0);
	    break;
#endif
#ifdef NEEDED
	case '3':
	    CloseSysopMenu(id);
	    lcd(0);
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts,
					" Privileged Functions ", 0);
	    break;
#endif
#ifdef NEED_MSG_SHOWING
	case '9':
	    CloseSysopMenu(id);
	    MsgShow();
	    iChar();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts,
					" Privileged Functions ", 0);
	    break;
#endif
#ifdef CHANGE_BAUDS
	case 'B':
	    changeBauds(id);
	    break;
#endif
	case 'E':
	    sprintf(systemPW, "%sabled\n ",
					(anyEcho = !anyEcho) ? "en" : "dis");
	    ScrNewUser();
	    SysopInfoReport(id, systemPW);
	    break;
	case 'F':
	    SysopRequestString(id, strFile, systemPW, 50, 0);
	    if (!ingestFile(systemPW, tempMess.mbtext)) {
		char *fn;

		fn = strdup(systemPW);
		sprintf(systemPW, NoFileStr, fn);
		SysopError(id, systemPW);
		free(fn);
	    }
	    else heldMess = TRUE;
	    break;
	case 'A':
	    killLogBuf(&lBuf);
	    CloseSysopMenu(id);
	    return GOOD_SELECT;
	case 'C':
	    sprintf(systemPW, "%sabled\n ",
		(cfg.BoolFlags.noChat = !cfg.BoolFlags.noChat)
		?
		"dis"
		:
		"en"
		);
	    ScrNewUser();
	    SysopInfoReport(id, systemPW);
	    break;
	case 'D':
	    cfg.BoolFlags.debug = !cfg.BoolFlags.debug;
	    sprintf(systemPW, "%sabled\n ",
					cfg.BoolFlags.debug ? "en" : "dis");
	    SysopInfoReport(id, systemPW);
	    break;
	case 'I':
	    sprintf(msgBuf.mbtext, " %s V%s%s\n Net version %s",
				VARIANT_NAME, VERSION, SysVers, netVersion);
	    sprintf(lbyte(msgBuf.mbtext), "\n Commands version %d.%d\n ", majorVers,
								fixVers);
	    sprintf(lbyte(msgBuf.mbtext), "ctdlcnfg.sys version %d\n ",
								cfg.paramVers);
	    ActiveEvents(msgBuf.mbtext);
#ifdef FAX_DEBUG
		strcat(msgBuf.mbtext, "\n ");
		if (FaxString != NULL) {
			sprintf(lbyte(msgBuf.mbtext), "Fax result code(s):\n ");
			AddFaxResults();
			sprintf(lbyte(msgBuf.mbtext), "Fax command: %s\n ",
								FaxString);
		}
		else sprintf(lbyte(msgBuf.mbtext), "Fax handling not enabled\n ");
#endif
	    SysopDisplayInfo(id, msgBuf.mbtext, " Info ");
	    break;
	case 'M':
	    CloseSysopMenu(id);
	    if (whichIO != MODEM) {
		whichIO = MODEM;
		setUp(FALSE);
	    }
	    printf("Chat mode %sabled\n ",
		cfg.BoolFlags.noChat  ?  "dis"  :  "en");
	    if (!gotCarrier()) {
		EnableModem(FALSE);
		ReInitModem();
	    }
	    killLogBuf(&lBuf);
	    ScrNewUser();
	    startTimer(NEXT_ANYNET);      /* start up anytime net timer */
	    if (gotCarrier()) mPrintf("System on-line\n ");
	    return GOOD_SELECT;
	case 'O':
	    CloseSysopMenu(id);
	    systemCommands();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts, " Privileged Functions ", 0);
	    break;
#ifdef MAKE_AVAILABLE
	case 'S':
	    changeDate();
	    break;
#endif
	case ERROR:
	case 'X':
	    if (!SysopGetYesNo(id, "", confirm)) break;
	    ExitToMsdos = TRUE;
	    exitValue   = (remoteSysop && !onConsole) ? REMOTE_SYSOP_EXIT : SYSOP_EXIT;
	    CloseSysopMenu(id);
	    return GOOD_SELECT;
	case 'N':
	    CloseSysopMenu(id);
	    netStuff();
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts, " Privileged Functions ", 0);
	    break;
	case 'R':
	    if (gotCarrier()) HangUp(TRUE);
	    Reinitialize();
	    break;
	case 'Q':
	    temp = SysopGetNumber(id, " Set oldest to", 0l, 200000l);
	    if (temp != 0l) cfg.oldest = temp;
	    break;
	case 'U':
	    CloseSysopMenu(id);
	    UserAdmin(&lBuf);
	    id = RegisterSysopMenu("ctdlopt.mnu", CtdlOpts, " Privileged Functions ", 0);
	    break;
	}
    }
    killLogBuf(&lBuf);
    CloseSysopMenu(id);
    return GOOD_SELECT;
}

/*
 * doUngoto()
 *
 * This function handles the Ungoto command.
 */
char doUngoto(char moreYet)
{
    label target;

    if (!moreYet) {
	strcpy(target, "");
    }
    else {
	if (getNormStr("", target, NAMESIZE, BS_VALID) == BACKED_OUT)
	    return BACKED_OUT;
    }
    retRoom(target);
    return GOOD_SELECT;
}

/*
 * getCommand()
 *
 * This function prints the menu prompt and gets command char and returns a
 * char via parameter and expand flag as value -- i.e., TRUE if parameters
 * follow else FALSE.
 */
char getCommand(char *c, char bs)
{
    char expand, again;

    outFlag = OUTOK;

    if (!bs)
	givePrompt();

    do {
	again = FALSE;

	/* bizarre cheat */
	if (!bs)
	    *c = (justLostCarrier) ? 0 : toUpper(modIn());
	else
	    *c = '.';

	expand  = (
	    *c == ' '
	    ||
	    *c == '.'
	    ||
	    *c == ','
	    ||
	    *c == '/'
	);

	if (expand) {
	    if (!bs) oChar(*c);
	    if ((*c = toUpper(modIn())) == '\b') {
		mPrintf("\b \b");
		again = TRUE;
	    }
	}
			/* catch a late Pause, et al, command */
	else if (*c == 'P' || *c == '\b') again = TRUE;
	else if (*c == 7) {
	    if (CheckForSpecial(13, 69))
		netController(0, 0, NO_NETS, ANY_CALL, 0);
	}
	/* else oChar(*c); -- actually, handled somewhere else! */
	bs = FALSE;
    } while (again && onLine());

    if (justLostCarrier) {
	justLostCarrier = FALSE;
	terminate(TRUE, TRUE);
	expand = 0;
    }
    return expand;
}

/*
 * greeting()
 *
 * This gives system-entry blurb etc.
 */
void greeting()
{
    extern char *VERSION, *SysVers;
    extern int logTries;

    if (loggedIn) terminate(FALSE, TRUE);

    setUp(TRUE);     pause(10);
    logTries = 1;	/* put this here instead of setUp() */

    memset(audit, ' ', AUDIT);
    acount = 0;

    PrintBanner = TRUE; /* signal for anytime net */

    doCR();
    expert = TRUE;

    printHelp("banner.pre", HELP_NO_HELP);
    if (!printHelp("banner", HELP_USE_BANNERS | HELP_NO_HELP))
	if (!printHelp("banner.blb", HELP_NO_HELP))
	    mPrintf("Welcome to %s\n", cfg.codeBuf + cfg.nodeTitle);
    printHelp("banner.sfx", HELP_NO_HELP);

    expert = FALSE;
    mPrintf(" Running: %s (V%s%s) \n  ", VARIANT_NAME, VERSION, SysVers);
    mPrintf(formDate());
    mPrintf("\n H for Help\n ");

    printf("Chat mode %sabled\n", cfg.BoolFlags.noChat ? "dis" : "en");
    printf("\n 'MODEM' mode.\n "			);
    printf("(<ESC> for CONSOLE mode.)\n "		);
    while (MIReady())
	Citinp();

    gotoRoom(baseRoom, MOVE_GOTO | MOVE_TALK);
    setUp(TRUE);

    PrintBanner = FALSE;
    if (outFlag == NET_CALL) {
	netController(0, 0, NO_NETS, ANY_CALL, 0);   /* so we don't call out */
    }
#ifdef STROLL_SUPPORT
    else if (outFlag == STROLL_DETECTED) {
	StrollIt();
    }
#endif
    outFlag = OUTOK;
}

#define FAXSTR	"faxstring="
/*
 * main()
 *
 * This is the main manager.
 */
void main(int argc, char **argv)
{
    extern char logNetResults, netDebug, DisVandals,
		VortexHandle, BpsSet, ItlWxmodem, IgnoreDoor, more[];
    extern char *UploadLog, LocalAreaCode;
    extern int pgdft;
    char c, x, errMsg;
    int  CmdResult = GOOD_SELECT;

    cfg.weAre		= CITADEL;
    slistmalloc		= GetDynamic;
    errMsg = FALSE;

    if ((UploadLog = getenv("DSZLOG")) == NULL) UploadLog = "";

    while (argc >= 2) {
	argc--;
	if (strCmpU(argv[argc], "+netlog") == SAMESTRING) {
	    logNetResults = TRUE;
	} else if (strncmp(argv[argc], "pgdft=", 6) == SAMESTRING) {
	    pgdft = atoi(argv[argc] + 6);
	} else if (strncmp(argv[argc], "mp=", 3) == SAMESTRING) {
	    if (strlen(argv[argc] + 3) < 15)
		strcpy(more, argv[argc] + 3);
	} else if (strncmp(argv[argc], "bps=", 4) == SAMESTRING) {
	    BpsSet = TRUE;
	    ReadBps(argv[argc]);
	} else if (strCmpU(argv[argc], "+localareacode") == SAMESTRING) {
	    LocalAreaCode++;
	} else if (strCmpU(argv[argc], "+netdebug") == SAMESTRING) {
/*	    printf("netdebug is on\n"); */
	    netDebug = TRUE;
	} else if (strCmpU(argv[argc], "+nochat") == SAMESTRING) {
	    NoChatAtAll = TRUE;
	} else if (strCmpU(argv[argc], "+noecho") == SAMESTRING) {
	    anyEcho = FALSE;
	} else if (strCmpU(argv[argc], "+wx") == SAMESTRING) {
#ifdef WXMODEM_AVAILABLE
	    ItlWxmodem = TRUE;
#else
	    printf("This version of %s does not support Wxmodem\n",
							VARIANT_NAME);
#endif
	} else if (strCmpU(argv[argc], "+vortex") == SAMESTRING) {
	    VortexHandle = TRUE;
	} else if (strCmpU(argv[argc], "+vandaloff") == SAMESTRING) {
	    DisVandals = TRUE;
	} else if (strCmpU(argv[argc], "+conpwd") == SAMESTRING) {
	    ConsolePassword = TRUE;
	} else if (strCmpU(argv[argc], "ignore-door") == SAMESTRING) {
	    IgnoreDoor = TRUE;
	} else if (strncmp(argv[argc], FAXSTR, strlen(FAXSTR)) == SAMESTRING) {
	    FaxString = argv[argc]+strlen(FAXSTR);
	} else if (!SystemDependentArgument(argv[argc])) {
	    printf("crash argument: %s\n", argv[argc]);
	    errMsg = TRUE;
	}
    }
    if (initCitadel()) {
	greeting();
	logMessage(FIRST_IN, 0l, 0);
    }

    startTimer(NEXT_ANYNET);      /* start anytime net timer */

    if (errMsg) {
	sprintf(msgBuf.mbtext, "System brought up from apparent crash.");
	aideMessage(NULL,FALSE);
    }

    while (!ExitToMsdos)  {
	x       = getCommand(&c, (CmdResult == BACKED_OUT));

	CurLine = 1;		/* should fix possible problems */
	outFlag = OUTOK;

	CmdResult = (c==CNTRLl)  ?  doSysop() : doRegular(x, c);
	if (CmdResult == BAD_SELECT) {
	    if (!expert)    mPrintf(" ? (Type '?' for menu)\n \n"   );
	    else	    mPrintf(" ?\n "			    );
	}
    }

    if (loggedIn)
	terminate( /* hangUp == */ exitValue == DOOR_EXIT ? FALSE : TRUE, TRUE);

    logMessage(exitValue != DOOR_EXIT ? LAST_OUT : DOOR_OUT , 0l, 0);

    writeSysTab(); 
    if (onConsole) EnableModem(FALSE);	/* just in case... */
    if (exitValue != DOOR_EXIT && (!cfg.BoolFlags.IsDoor || IgnoreDoor))
	DisableModem(TRUE);
    ModemShutdown(((exitValue == DOOR_EXIT ||
	(cfg.BoolFlags.IsDoor && !IgnoreDoor)) && !onConsole) ? FALSE : TRUE);
    systemShutdown(0);
    exit(exitValue);
}

/*
 * UserAdmin()
 *
 * This function handles the user administration menu.
 */
void UserAdmin(logBuffer *lBuf)
{
    int      logNo, ltabSlot, result;
    char     work[70];
    label    who;
    MenuId   id;
    char     *UserOpts[] = {
	"Add new user\n", "Door privs\n", "Endless User (permanent account)\n",
	"File privs\n", "Kill user\n", "Net privs\n", "Privileges (aide)\n",
	"Twit\n", "X\beXit\n",
	""
    };

    id = RegisterSysopMenu("useropt.mnu", UserOpts, " User Administration ", 0);
    while (onLine()) {
	outFlag = OUTOK;
	SysopMenuPrompt(id, "\n user admin fn: ");

	switch (GetSysopMenuChar(id)) {
	case ERROR:
	case 'X': CloseSysopMenu(id); return ;
	case 'E':	/* permanent account administration */
	    if ((logNo = GetUser(who, lBuf, TRUE)) == ERROR ||
			logNo == cfg.MAXLOGTAB) break;
	    sprintf(work, "%s %s a permanent account\n ", lBuf->lbname,
			lBuf->lbflags.PERMANENT ? "does not have" : "has");

	    if (!SysopGetYesNo(id, work, confirm))   break;

	    lBuf->lbflags.PERMANENT = !lBuf->lbflags.PERMANENT;

	    putLog(lBuf, logNo);

	    /* find position in logTab[] and update that, too */
	    if ((ltabSlot = PWSlot(lBuf->lbpw, /* load == */ FALSE)) != ERROR)
		logTab[ltabSlot].ltpermanent = lBuf->lbflags.PERMANENT;

	    if (loggedIn  &&  strCmpU(logBuf.lbname, who)==SAMESTRING)
		logBuf.lbflags.PERMANENT = lBuf->lbflags.PERMANENT;

 	    break;
	case 'T':
	    if ((logNo = GetUser(who, lBuf, TRUE)) == ERROR ||
				logNo == cfg.MAXLOGTAB) break;
	    sprintf(work, "%s is %sa twit\n ", lBuf->lbname,
				lBuf->lbflags.TWIT ? "not " : "");

	    if (!SysopGetYesNo(id, work, confirm))   break;

	    lBuf->lbflags.TWIT = !lBuf->lbflags.TWIT;

	    putLog(lBuf, logNo);

	    if (loggedIn  &&  strCmpU(logBuf.lbname, who)==SAMESTRING)
		logBuf.lbflags.TWIT = lBuf->lbflags.TWIT;

	    break;
	case 'F':
	    if ((logNo = GetUser(who, lBuf, TRUE)) == ERROR) break;
	    if (logNo == cfg.MAXLOGTAB) {
		result = DoAllQuestion("Give everyone file privs",
					"Take away everyone's file privs");
		if (result == ERROR) break;
		for (logNo = 0; logNo < cfg.MAXLOGTAB; logNo++) {
		    getLog(lBuf, logNo);
		    if (!onConsole) mPrintf(".");
		    if (lBuf->lbflags.L_INUSE && lBuf->lbflags.DL_PRIVS != result) {
			lBuf->lbflags.DL_PRIVS = result;
			putLog(lBuf, logNo);
		    }
		}
		break;
	    }
	    sprintf(work, "%s has %sfile privs\n ", lBuf->lbname,
				lBuf->lbflags.DL_PRIVS ? "no " : "");

	    if (!SysopGetYesNo(id, work, confirm))   break;

	    lBuf->lbflags.DL_PRIVS = !lBuf->lbflags.DL_PRIVS;

	    putLog(lBuf, logNo);

	    if (loggedIn  &&  strCmpU(logBuf.lbname, who)==SAMESTRING)
		logBuf.lbflags.DL_PRIVS = lBuf->lbflags.DL_PRIVS;

	    break;
	case 'K':
	    if ((logNo = GetUser(who, lBuf, TRUE)) == ERROR ||
				logNo == cfg.MAXLOGTAB) break;
	    if (lBuf->credit != 0)
		sprintf(work, "%s has %d credit for l-d!", who);
	    else sprintf(work, "Kill %s", who);
	    if (!SysopGetYesNo(id, work, confirm))   break;
	    ltabSlot = PWSlot(lBuf->lbpw, /* load == */ FALSE);

	    RemoveUser(logNo, lBuf);

	    lBuf->lbname[0] = '\0';
	    lBuf->lbpw[0  ] = '\0';
	    lBuf->lbflags.L_INUSE = FALSE;

	    putLog(lBuf, logNo);

	    logTab[ltabSlot].ltpwhash       = 0;
	    logTab[ltabSlot].ltnmhash       = 0;

	    break;
	case 'P':
	    if ((logNo = GetUser(who, lBuf, TRUE)) == ERROR ||
				logNo == cfg.MAXLOGTAB) break;

	    if (lBuf->lbflags.AIDE == 1) {
		lBuf->lbflags.AIDE = 0;
		lBuf->lbrgen[AIDEROOM] = ((roomTab[AIDEROOM].rtgen-1) % MAXGEN);
	    }
	    else {
		lBuf->lbflags.AIDE = 1;
		lBuf->lbrgen[AIDEROOM] = roomTab[AIDEROOM].rtgen;
	    }
	    sprintf(work,
		"%s %s aide privileges\n ",
		who,
		(lBuf->lbflags.AIDE == 1)  ?  "gets"  :  "loses"
	    );
	    if (!SysopGetYesNo(id, work, confirm))   break;

	    putLog(lBuf, logNo);

	    /* see if it is us: */
	    if (loggedIn  &&  strCmpU(logBuf.lbname, who)==SAMESTRING)   {
		aide = (lBuf->lbflags.AIDE == 1) ? TRUE : FALSE;
		logBuf.lbrgen[AIDEROOM] = lBuf->lbrgen[AIDEROOM];
	    }
	    break;
	case 'D':
	    if ((logNo = GetUser(who, lBuf, TRUE)) == ERROR) break;
	    if (logNo == cfg.MAXLOGTAB) {
		result = DoAllQuestion("Give everyone door privs",
					"Take away everyone's door privs");
		if (result == ERROR) break;
		for (logNo = 0; logNo < cfg.MAXLOGTAB; logNo++) {
		    getLog(lBuf, logNo);
		    if (!onConsole) mPrintf(".");
		    if (lBuf->lbflags.L_INUSE && lBuf->lbflags.DOOR_PRIVS != result) {
			lBuf->lbflags.DOOR_PRIVS = result;
			putLog(lBuf, logNo);
		    }
		}
		break;
	    }
	    lBuf->lbflags.DOOR_PRIVS  = !lBuf->lbflags.DOOR_PRIVS;
	    sprintf(work, "%s %s door privileges\n ", who,
		(lBuf->lbflags.DOOR_PRIVS) ?  "gets"  :  "loses"
	    );

	    if (!SysopGetYesNo(id, work, confirm))   break;

	    putLog(lBuf, logNo);

	    /* see if it is us: */
	    if (loggedIn  &&  strCmpU(logBuf.lbname, who)==SAMESTRING) {
		DoorPriv = lBuf->lbflags.DOOR_PRIVS;
	    }
	    break;
	case 'N':
	    NetPrivs(who);
	    break;
	case 'A':
	    CloseSysopMenu(id);
	    newUser(&logTmp);
	    id = RegisterSysopMenu("useropt.mnu", UserOpts, " User Administration ", 0);
	    break;
	}
    }
}

lcd(int x)
{
#ifdef NEEDED
	int rover;
	    extern MSG_NUMBER    *lPtrTab;

		mPrintf("From location %d\n ", x);
#ifdef LOOP
		for (rover = 0; rover < MAXROOMS; rover++) {
#else
	rover = 55;
#endif
if (logBuf.lastvisit[rover] == lPtrTab[rover]) return;
		PagingOn();
			mPrintf("%3d. %ld, %ld\n ", rover,
						logBuf.lastvisit[rover],
						lPtrTab[rover]);
#ifdef LOOP
		}
#endif
		PagingOff();
		iChar();
#endif
}
