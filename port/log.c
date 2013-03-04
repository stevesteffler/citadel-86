/*
 *				log.c
 *
 * userlog code for Citadel bulletin board system.
 */

/*
 *				history
 *
 * 85Nov15 HAW  MS-DOS library implemented.
 * 85Aug31 HAW  Fix <.ep> problem.
 * 85Aug17 HAW  Update to onLine().
 * 85Aug10 HAW  Fix so system doesn't go out to disk on short pwds.
 * 85Jul26 HAW  Kill noteLog(), insert anti-hack code in newPW().
 * 85Jun13 HAW  Tweak code for networking stuff.
 * 85Mar13 HAW  Moved zapLogFile() and logInit and logSort into confg.c.
 * 85Jan19 HAW  Fix terminate() so room prompt isn't tossed at modem.
 * 85Jan19 HAW  New Users are now directed to type ".help POLICY".
 * 85Jan19 HAW  Move findPerson() into file.
 * 84Dec15 HAW  Fix bug that allowed discovery of private rooms.
 * 84Aug30 HAW  Now we roll into the 16-bit world.
 * 84Jun23 HAW&JLS  Eliminating unused local variables using CRF.
 * 84Jun19 JLS  Fixed terminate so that Mail> doesn't screw up SYSOP.
 * 84Apr04 HAW  Started upgrade to BDS C 1.50a. 
 * 83Feb27 CrT  Fixed login-in-Mail> bug.
 * 83Feb26 CrT  Limited # new messages for new users.
 * 83Feb18 CrT  Null pw problem fixed.
 * 82Dec06 CrT  2.00 release.
 * 82Nov03 CrT  Began local history file & general V1.2 cleanup
 */

#define INTERRUPTED_MESSAGES
#define LOGIN

#include "ctdl.h"

/*
 *				contents
 *
 *      doInviteDisplay()       displays invited users
 *      GetUser()		gets a user record from user
 *      login()			is menu-level routine to log caller in
 *      newPW()			is menu-level routine to change a PW
 *      newUser()		menu-level routine to log a new caller
 *      PWSlot()		returns CTDLLOG.buf slot password is in
 *      slideLTab()		support routine for sorting logTab
 *      storeLog()		store data in log
 *      strCmpU()		strcmp(), but ignoring case distinctions
 *      terminate()		menu-level routine to exit system
 */

int		thisSlot;		/* logTab slot logBuf found via */
char		loggedIn = FALSE;      /* Global have-caller flag      */
char		prevChar;		/* for EOLN/EOParagraph stuff   */
static char      pwChangeCount;		/* Anti-hack variable		*/
int		logTries;
char *AbortAcct = "Abort account creation";

extern CONFIG    cfg;		/* Configuration variables      */
extern logBuffer logBuf;		/* Log buffer of a person       */
extern logBuffer logTmp;		/* Log buffer of a person       */
extern LogTable  *logTab;		/* RAM index of pippuls		*/
LogTable         *delogTab = NULL;	/* RAM index of pippuls		*/
extern rTable    *roomTab;		/* RAM index of rooms		*/
extern aRoom     roomBuf;		/* Room buffer		*/
extern MessageBuffer   tempMess, msgBuf;
extern char	 heldMess;
extern FILE	 *logfl;		/* log file descriptor		*/
extern int	 thisLog;		/* entry currently in logBuf    */
extern char	 outFlag;		/* Output skip flag		*/
extern char	 whichIO;		/* Where IO's going...		*/
extern char	 haveCarrier;       /* Do we still got carrier?     */
extern char	 echo;		/* Who gets what		*/
extern char	 onConsole;		/* Where we get stuff from      */
extern int	 thisRoom;		/* The room we're in		*/
extern int	 exitValue;
extern char	 DiskHeld;
extern char	 *LCHeld, shownHidden;
extern long	 *DL_Total;
extern SListBase DL_List;

#define HAVE_HELD	\
"\n  * You have a message in your Hold buffer. Use .EH to access it. *\n "

/*
 * doInviteDisplay()
 *
 * This function shows invited users.
 */
char doInviteDisplay()
{
    int rover = 0, s;
    label who;

    if (getNormStr("", who, NAMESIZE, BS_VALID) == BACKED_OUT)
	return BACKED_OUT;

    if (strlen(who) == 0) {
	mPrintf("%s, ", logBuf.lbname);
	outFlag = OUTOK;
	for (; outFlag == OUTOK && rover < cfg.MAXLOGTAB; rover++) {
	    if (rover != thisLog) {
		getLog(&logTmp, rover);
		if (logTmp.lbflags.L_INUSE &&
			(s = knowRoom(&logTmp, thisRoom)) == KNOW_ROOM ||
			s == WRITE_PRIVS) {
		    mPrintf("%s", logTmp.lbname);
		    if (s == WRITE_PRIVS && roomBuf.rbflags.READ_ONLY)
			mPrintf(" (w-p)");
		    mPrintf(", ");
		}
	    }
	}
	mPrintf("\b\b.");
    }
    else {
	if (findPerson(who, &logTmp) == ERROR)
	    mPrintf("Who?\n ");
	else if ((s = knowRoom(&logTmp, thisRoom)) == KNOW_ROOM ||
			s == WRITE_PRIVS)
	    mPrintf("Yup!\n ");
	else
	    mPrintf("Nope!\n ");
    }
    return GOOD_SELECT;
}

/*
 * GetUser()
 *
 * This is a general function for requesting user name, getting user record,
 * and doing error checking.  Returns ERROR on failure.
 */
int GetUser(label who, logBuffer *lBuf, char Menu)
{
    int logNo;

    if (Menu && onConsole)
	 SysopRequestString(NO_MENU, "who", who, NAMESIZE, 0);
    else getNormStr("who", who, NAMESIZE, 0);
    if (strlen(who) == 0) return ERROR;

    if (strCmpU(who, "Citadel") == SAMESTRING)
	return cfg.MAXLOGTAB;

    if (strCmpU(who, "Sysop") == SAMESTRING)
	strcpy(who, cfg.SysopName);

    logNo = findPerson(who, lBuf);
    if (logNo == ERROR) {
	if (Menu && onConsole)
	     SysopError(NO_MENU, "No such person\n ");
	else mPrintf("No such person\n ");
    }

    return logNo;
}

/*
 * DoAllQuestion()
 *
 * This will ask if someone wants to apply an operation to all users.
 */
int DoAllQuestion(char *posprompt, char *negprompt)
{
	if (SysopGetYesNo(NO_MENU, "", posprompt)) return TRUE;
	if (SysopGetYesNo(NO_MENU, "", negprompt)) return FALSE;
	return ERROR;
}

/*
 * login()
 *
 * This is the menu-level routine to log someone in.
 */
void login(char *password)
{
	extern OptValues Opt;
	int  foundIt;
	label name;
	TwoNumbers *tmp;

	if (cfg.BoolFlags.ParanoidLogin) {
		if (strlen(password))
			getNormStr("account name", name, NAMESIZE, 0);
	}
	foundIt =    ((PWSlot(password, /*load = */TRUE)) != ERROR);
	pwChangeCount = 1;

	if (foundIt && *password && (!cfg.BoolFlags.ParanoidLogin ||
		strCmpU(logBuf.lbname, name) == SAMESTRING)) {
		if (!LoggedInDoor()) {
			HangUp(TRUE);
			return;
		}

		/* update userlog entries: */

		heldMess     = FALSE;
		loggedIn     = TRUE;
		setUp(TRUE);

lcd(1);
		/* recite caller's name, etc:    */
		mPrintf(" %s\n", logBuf.lbname);

		if (!NoLoginSignal() && SearchList(&ChatOn, logBuf.lbname))
			CallChat(1, FALSE);

lcd(2);
		if (RunAutoDoor(CheckAutoDoor(logBuf.lbname), FALSE)) return;
lcd(3);

		ScrNewUser();
lcd(4);
		printHelp("notice.pre", HELP_NO_HELP|HELP_LEAVE_PAGEABLE);
		if (!printHelp("notice", HELP_USE_BANNERS |
					HELP_NO_HELP|HELP_LEAVE_PAGEABLE))
			printHelp("notice.blb",
					HELP_NO_HELP|HELP_LEAVE_PAGEABLE);
		printHelp("notice.sfx", HELP_NO_HELP|HELP_LEAVE_PAGEABLE);

		logMessage(L_IN, 0l, 0);
lcd(5);

		zero_struct(Opt);
		Opt.Date = -1l;
		Opt.MaxMessagesToShow = -1;
		showMessages(MSG_LEAVE_PAGEABLE | PAGEABLE | NEWoNLY,
			logBuf.lastvisit[thisRoom], OptionValidate);
lcd(6);

		listRooms(expert ? INT_EXPERT : INT_NOVICE);
lcd(7);
		if (shownHidden && !expert)
			mPrintf("\n \n * => hidden room\n ");

	/*
	 * This code sets up to handle download time limits.  Users who
	 * download are kept in a list -- user# + time spent in downloading.
	 * The list is cleared whenever the system is brought down/up, or
	 * a d-l time limit #event expires or initiates.  Here, we check to
	 * see if this user has downloaded during the current period.  If so,
	 * we simply point at the record so we know how much time has already
	 * been spent downloading.  If not found, we initialize a new record.
	 */

		if ((DL_Total = SearchList(&DL_List, (tmp = MakeTwo(thisLog, 0l))))
							== NULL) {
			DL_Total = &tmp->second;
			AddData(&DL_List, tmp, NULL, TRUE);
		}
		else free(tmp);

lcd(8);
		outFlag = OUTOK;
		if (thisRoom != MAILROOM && RoomHasNew(MAILROOM)) {
			mPrintf("\n  * You have private mail in Mail> *\n ");
		}

		if (GetIntMessage()) {
			outFlag = OUTOK;
			DiskHeld = TRUE;
			mPrintf(HAVE_HELD);
		}
lcd(9);
	} else {
		setUp(FALSE);
		/* discourage password-guessing: */
		if (cfg.LoginAttempts > 0 && logTries++ >= cfg.LoginAttempts)
			if (whichIO == MODEM) {
				printf("Login attempts exceeded, carrier dropped.\n");
				HangUp(TRUE);
				return ;	/* skip the pause() farther on*/
			}
		if (strlen(password) > 1 && whichIO == MODEM)
			if ((logTries-1) > 1) pause(2000);
		if (!cfg.BoolFlags.unlogLoginOk  &&  whichIO == MODEM)  {
			if (!printHelp("unlog.blb", 0))
				mPrintf(" No record -- leave message to 'sysop' in Mail>\n ");
		} else {
			if (getYesNo(" No record: Enter as new user")) {
				newUser(&logBuf);
			}
		}
	}
}

/*
 * GetIntMessage()
 *
 * This will get an interrupted message from the Held dir.
 */
char GetIntMessage()
{
    extern FILE *upfd;
    SYS_FILE funFile;
    label temp;

    /* if the user used Pause-E, we don't want to flush the buffer */
    if (!heldMess)
	ZeroMsgBuffer(&tempMess);

    if (cfg.BoolFlags.HoldOnLost) {
	sprintf(temp, LCHeld, thisLog);
	makeSysName(funFile, temp, &cfg.holdArea);
	if ((upfd = fopen(funFile, READ_ANY)) != NULL) {
	    fread(&tempMess, STATIC_MSG_SIZE, 1, upfd);
	    crypte(&tempMess, STATIC_MSG_SIZE, thisLog);
	    fread(tempMess.mbtext, MAXTEXT, 1, upfd);
	    crypte(tempMess.mbtext, MAXTEXT, thisLog);
	    MakeList(&tempMess.mbCC, "", upfd);
	    fclose(upfd);
	    unlink(funFile);
	    heldMess = TRUE;
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * newPW()
 *
 * This is menu-level routine to change one's password.  Since some Citadel
 * nodes run in public locations, we avoid displaying passwords on the console.
 */
void newPW()
{
    char oldPw[NAMESIZE];
    char pw[NAMESIZE];
    int  goodPW;
    extern char *confirm;

    if (!loggedIn) {
	mPrintf("\n How?\n ");
	return ;
    }

    /* save password so we can find current user again: */
    strcpy(oldPw, logBuf.lbpw);
    storeLog();
    do {
	echo    = CALLER;
	getNormStr(" new password", pw, NAMESIZE, NO_ECHO);
	echo    = BOTH;

	if (strlen(pw) == 0 || strCmpU(pw, oldPw) == SAMESTRING) {
	    PWSlot(oldPw, /*load = */TRUE);
	    return ;
	}

	/* check that PW isn't already claimed: */
	goodPW = (PWSlot(pw,/* load = */TRUE) == ERROR  &&  strlen(pw) >= 2 && strCmpU(pw, logBuf.lbname) != 0);

	if (pwChangeCount == 0 && !onConsole && onLine()) {
	    mPrintf("Hang on....\n");
	    pause(3000);	    /* Discourage hacking       */
	}
	else pwChangeCount--;

	if (!goodPW) mPrintf("\n Poor password\n ");

    } while (!goodPW && (haveCarrier || whichIO==CONSOLE));

    doCR();
    PWSlot(oldPw, /*load = */TRUE);     /* reload old log entry	*/
    pw[NAMESIZE-1] = 0x00;		/* insure against loss of carrier:*/

    if (goodPW  &&  strlen(pw) > 1) {   /* accept new PW:		*/
	mPrintf("\n %s\n pw: ", logBuf.lbname);
	echo = CALLER;
	mPrintf("%s\n ", pw);
	echo = BOTH;
	if (getYesNo(confirm)) {
	    strcpy(logBuf.lbpw, pw);
	    logTab[0].ltpwhash      = hash(pw);
	    storeLog();
	}
    }
}

/*
 * newUser()
 *
 * This is the add new user function.
 */
void newUser(logBuffer *lBuf)
{
    logBuffer   l2;
    char	*temp;
    int BadUserName(label fullnm);
    char	fullnm[NAMESIZE], tmp[30];
    SYS_FILE    checkHeld;
    char	pw[NAMESIZE], NewLogin;
    int		h, i = 0, ourSlot, AttemptCount = 0;
    int		good;	/* does double duty, keep it as int */
    MSG_NUMBER  low;
    TwoNumbers *ttemp;
    extern char *LCHeld;
    extern int  pgdft;

    if (!NewUserDoor()) {
	HangUp(TRUE);
	return;
    }

    NewLogin = (lBuf == &logBuf) ;

    if (NewLogin)
	printHelp("newuser.blb", 0);

    zero_struct(lBuf->lbflags);			/* zero all flags */
    lBuf->lbflags.LFMASK = TRUE;		/* except we need linefeeds */
    if (!configure(lBuf, !NewLogin, TRUE))	/* configure correctly */
	return;

    initLogBuf(&l2);

    if (!expert)   printHelp("password.blb", HELP_SHORT | HELP_BITCH);

    do {
	/* get name and check for uniqueness... */
	if (AttemptCount++ > cfg.LoginAttempts)
	    mPrintf("Please make up your mind.\n ");
	if (AttemptCount > cfg.LoginAttempts + 2 && NewLogin)
	    if (whichIO == MODEM) HangUp(TRUE);

	do {
	    getNormStr(" Name", fullnm, NAMESIZE, 0);
	    if ((temp = strchr(fullnm, '@')) != NULL) *temp = 0;
	    NormStr(fullnm);
	    good = TRUE;
	    if (findPerson(fullnm, &l2) != ERROR)
		good = FALSE;
	    if (BadUserName(fullnm))
		good = FALSE;
	    h = hash(fullnm);
	    if (
		h == 0		/* "HUH?" --HAW 84Aug31		*/
		||
		h == hash("Citadel")
		||
		h == hash("Sysop")
	    ) {
		good = FALSE;
	    }
	    /* lie sometimes -- hash collision !=> name collision */
	    if (!good) {
		if (strlen(fullnm) == 0) {
		    if (getYesNo(AbortAcct)) {
			killLogBuf(&l2);
			setUp(NewLogin);
			logTries++;
			return ;
		    }
		}
		else mPrintf("We already have a %s\n", fullnm);
	    }
	} while (!good  &&  (haveCarrier || whichIO==CONSOLE));

	/* get password and check for uniqueness...     */
	do {
	    echo	= CALLER;
	    getNormStr(" password (19 chars max)",  pw, NAMESIZE, 0);
	    echo	= BOTH  ;

	    h    = hash(pw);
	    for (i = 0, good = (strlen(pw) > 1 && strCmpU(pw, fullnm) != 0);
		i < cfg.MAXLOGTAB && good;
		i++) {
		if (h == logTab[i].ltpwhash) good = FALSE;
	    }
	    if (h == 0)   good = FALSE;
	    if (!good) {
		if (strlen(pw) == 0) {
		    if (getYesNo(AbortAcct)) {
			killLogBuf(&l2);
			setUp(NewLogin);
			logTries++;
			return ;
		    }
		}
		else mPrintf("\n Poor password\n ");
	    }
	} while( !good  &&  (haveCarrier || whichIO==CONSOLE));

	mPrintf("\n name: %s", fullnm);
	mPrintf("\n password: ");
	echo = CALLER;
	mPrintf("%s\n ", pw);
	echo = BOTH;
    } while (!getYesNo("OK") && (haveCarrier || whichIO==CONSOLE));


    if ((haveCarrier || whichIO == CONSOLE)) {
	/* kick least recent temporary acct out of userlog and claim entry: */
	for (good = cfg.MAXLOGTAB-1; good >= 0; good--)
	    if (!logTab[good].ltpermanent) break;
	if (NewLogin) lBuf->lbpage = pgdft;

	if (good < 0) good = cfg.MAXLOGTAB - 1;	/* too bad */

	ourSlot		= logTab[good].ltlogSlot;

	slideLTab((NewLogin) ? 0 : 1, good);
	logTab[NewLogin ? 0 : 1].ltlogSlot = ourSlot;

	if (NewLogin)
	    thisLog = ourSlot;

        /* get old record so we can remove the dead user from system records */
	getLog(&l2, ourSlot);
	if (l2.lbflags.L_INUSE)
		RemoveUser(ourSlot, &l2);

	/* copy info into record:       */
	strcpy(lBuf->lbname, fullnm);
	strcpy(lBuf->lbpw, pw);
	lBuf->lbflags.L_INUSE    = TRUE;
	lBuf->lbflags.NET_PRIVS  = cfg.BoolFlags.NetDft;
	lBuf->lbflags.DOOR_PRIVS = cfg.BoolFlags.DoorDft;
	lBuf->lbflags.DL_PRIVS   = cfg.BoolFlags.DL_Default;
	lBuf->lbdelay            = cfg.ECD_Default;
	lBuf->credit		 = 0;	/* No L-D credit	*/

	/*
	 * this has to be done after the name is copied 'cuz logMessage
	 * uses the logBuf global to find the name of the user.
	 */
	if (NewLogin)
	    logMessage(L_IN, 0l, LOG_NEWUSER);

	low = cfg.newest-50;
	if (cfg.oldest - low < 0x8000)   low = cfg.oldest;

	/* initialize rest of record:   */
	for (i = 0;  i < MAXROOMS;  i++) {
	    lBuf->lastvisit[i] = 0l;
	    if (roomTab[i].rtflags.PUBLIC == 1) {
		lBuf->lbrgen[i] = roomTab[i].rtgen;
	    } else {
		/* set to one less */
		lBuf->lbrgen[i] = (roomTab[i].rtgen + (MAXGEN-1)) % MAXGEN;
	    }
	}
	memset(lBuf->lbMail, 0, MAIL_BULK);

	/* fill in logTab entries       */
	i = (NewLogin) ? 0 : 1;
	logTab[i].ltpwhash      = hash(pw)	;
	logTab[i].ltnmhash      = hash(fullnm)    ;
	logTab[i].ltlogSlot     = ourSlot	;
	logTab[i].ltnewest      = 0l;
	logTab[i].ltpermanent   = FALSE;	/* new accts not perm */

	/* special kludge for Mail> room, to signal no new mail:   */
	if (NewLogin) {
	    loggedIn = TRUE;

	    roomTab[MAILROOM].rtlastMessage =
					lBuf->lbMail[MAILSLOTS-1].rbmsgNo;
	}

	/* automatic mail for the new user. */
	makeSysName(checkHeld, (lBuf->lbflags.EXPERT) ? "expmail.blb" :
					"novmail.blb", &cfg.homeArea);
	if (access(checkHeld, 0) == 0) {
	    i = thisRoom;
	    getRoom(MAILROOM);
	    ZeroMsgBuffer(&msgBuf);
	    ingestFile(checkHeld, msgBuf.mbtext);
	    strcpy(msgBuf.mbauth, (strlen(cfg.SysopName)) ?
				    cfg.SysopName : "Citadel");
	    strcpy(msgBuf.mbto, lBuf->lbname);
	    putMessage(lBuf, /* SKIP_AUTHOR */ 0);
	    getRoom(i);
	}

	if (NewLogin) {
	    storeLog();

	    ScrNewUser();

	    if (expert && !cfg.BoolFlags.NoMeet) {
		if (getYesNo("Would you care to write a biography (this can be done later)"))
		    EditBio();
	    }
	    /*
	     * Create a new download record.  See login() for more details.
	     */
	    AddData(&DL_List, ttemp = MakeTwo(thisLog, 0l), NULL, TRUE);
	    DL_Total = &ttemp->second;
	}

	if (cfg.BoolFlags.HoldOnLost) {
	    sprintf(tmp, LCHeld, ourSlot);
	    makeSysName(checkHeld, tmp, &cfg.holdArea);
	    unlink(checkHeld);
	}

	if (NewLogin) {
	    setUp(FALSE);
	    if (!expert) {
		makeSysName(checkHeld, "guide.hlp", &cfg.homeArea);
		if (access(checkHeld, 0) == 0)
		    if (getYesNo("Would you like to see a Novice Help Guide"))
			printHelp("guide.hlp", HELP_NO_LINKAGE);
	    }
	    if (!printHelp("notice", HELP_USE_BANNERS))
		printHelp("notice.blb", 0);
	    listRooms(expert ? INT_EXPERT : INT_NOVICE);
	    if (thisRoom != MAILROOM &&
		RoomHasNew(MAILROOM)
		)   {
		mPrintf("\n  * You have private mail in Mail> *\n ");
	    }
	    mPrintf("\n \n Please type \".Help POLICY\"\n ");
	}
	else {
	    logTab[1].ltnewest    = cfg.newest;
	    lBuf->lbflags.DOOR_PRIVS = getYesNo("Give door privileges");
	    if (cfg.BoolFlags.netParticipant)
		lBuf->lbflags.NET_PRIVS = getYesNo("Give net privileges");
	    lBuf->lbflags.DL_PRIVS = getYesNo("Give download privileges");

	    putLog(lBuf, ourSlot);
	}
    }
    killLogBuf(&l2);
}

/*
 * BadUserName()
 *
 * This function searches badnames.sys to discover if the requested name is
 * acceptable or not.
 */
static int BadUserName(label fullnm)
{
    SYS_FILE name;
    char line[25], toReturn = FALSE;
    FILE *fd;

    makeSysName(name, "badnames.sys", &cfg.roomArea);
    if ((fd = fopen(name, READ_TEXT)) != NULL) {
	while (GetAString(line, sizeof line, fd) != NULL && !toReturn)
	    if (strCmpU(fullnm, line) == 0)
		toReturn = TRUE;
	fclose(fd);
    }
    return toReturn;
}

/*
 * PWSlot()
 *
 * This returns ctdllog.sys slot password is in, else ERROR.
 * NB: we also leave the record for the user in logBuf.
 */
int PWSlot(char pw[NAMESIZE], char load)
{
    int  h, i;
    int  foundIt, ourSlot;

    if (strlen(pw) < 2)		/* Don't search for these pwds		*/
	return ERROR;

    h = hash(pw);

    /* Check all passwords in memory: */
    for(i = 0, foundIt = FALSE;  !foundIt && i < cfg.MAXLOGTAB;  i++) {
	/* check for password match here */

	/* If password matches, check full password	*/
	/* with current newUser code, password hash collisions should   */
	/* not be possible... but this is upward compatable & cheap     */
	if (logTab[i].ltpwhash == h) {
	    ourSlot     = logTab[i].ltlogSlot;
	    getLog(&logTmp, ourSlot);

	    if (strCmpU(pw, logTmp.lbpw) == SAMESTRING) {
		/* found a complete match */
		thisSlot = i   ;
		foundIt  = TRUE;
	    }
	}
    }
    if (foundIt) {
	if (load == TRUE) {
	    copyLogBuf(&logTmp, &logBuf);
	    thisLog = ourSlot;
	}
	return thisSlot;
    }
    return ERROR   ;
}

/*
 * slideLTab()
 *
 * This slides bottom N lots in logTab down.  For sorting.
 */
void slideLTab(int slot, int last)
{
    int i;

    /* open slot up: (movmem isn't guaranteed on overlaps) */
    for (i = last - 1;  i >= slot;  i--)  {
	memcpy(&logTab[i + 1], logTab + i, sizeof *logTab);
    }
}

/*
 * storeLog()
 *
 * This function stores the current log record.
 */
void storeLog()
{
    logTab[0].ltnewest    = logBuf.lblaston;
    putLog(&logBuf, thisLog);
}

/*
 * strCmpU()
 *
 * This is strcmp(), but ignoring case distinctions.  Really, it's stricmp().
 */
int strCmpU(char s[], char t[])
{
    int  i;

    i = 0;

    while (toUpper(s[i]) == toUpper(t[i])) {
	if (s[i++] == '\0')  return 0;
    }
    return  toUpper(s[i]) - toUpper(t[i]);
}

/*
 * terminate()
 *
 * This is the menu-level routine for a user to exit system.
 */
void terminate(int discon, int save)
{
    extern MSG_NUMBER    *lPtrTab;
    extern char heldMess, IgnoreDoor;
    extern MessageBuffer	tempMess;
    extern int	exitValue;
    extern char	ExitToMsdos;
    int	i;
    char	StillThere;

    outFlag    = IMPERVIOUS;
    DiskHeld   = FALSE;
    StillThere = onLine();

		/* this stops the .TS cheat of password hacking */
    if (!loggedIn) discon = TRUE;

    if (exitValue != DOOR_EXIT) {
	if (loggedIn)
	    mPrintf(" %s logged out\n ", logBuf.lbname);

	if (StillThere) {
	    printHelp("lonotice.pre", HELP_NO_HELP | HELP_NOT_PAGEABLE);
	    if (!printHelp("lonotice",
			HELP_USE_BANNERS | HELP_NO_HELP | HELP_NOT_PAGEABLE))
		printHelp("lonotice.blb", HELP_NO_HELP | HELP_NOT_PAGEABLE);
	    printHelp("lonotice.sfx", HELP_NO_HELP | HELP_NOT_PAGEABLE);
	}

	if (discon)  {
	    switch (whichIO) {
	    case MODEM:
		if (onLine() && (!cfg.BoolFlags.IsDoor || IgnoreDoor)) {
		    HangUp(FALSE);
		    modIn();		/* And now detect carrier loss  */
		    pause(50);
		    while (MIReady()) Citinp();    /* clear buffer */
		}
		break;
	    case CONSOLE:
		whichIO =  MODEM;
		EnableModem(FALSE);
		if (!ExitToMsdos) {
		    ReInitModem();
		    printf("\n'MODEM' mode.\n ");
		}
		startTimer(NEXT_ANYNET);    /* start up anytime net timer */
		break;
	    }

	    /* if we're functioning as a door ... */
	    if (cfg.BoolFlags.IsDoor && !IgnoreDoor) {
		ExitToMsdos = TRUE;
		exitValue   = SYSOP_EXIT;   /* Seems appropriate */
	    }
	}
	logMessage(L_OUT, 0l, (discon) ? 0 : LOG_TERM_STAY);
    }

    printf("%s @ %s\n", formDate(), Current_Time());

    if (loggedIn) {
	ReadDate(formDate(), &logBuf.lblaston);
	if (StillThere && exitValue != DOOR_EXIT) {
	    SetKnown(-1, thisRoom, &logBuf);
	    logBuf.lastvisit[thisRoom] = cfg.newest;
	}

	if (!save)
	    for (i = 0; i < MAXROOMS; i++)
		logBuf.lastvisit[i] = lPtrTab[i];

	storeLog();
	logBuf.lbname[0] = 0;	/* For screen display */
	if (heldMess)
	    SaveInterrupted(&tempMess);
    }

    loggedIn = FALSE;

    setUp(TRUE);

    for (i = 0; i < MAXROOMS; i++)      /* Clear skip bits */
	roomTab[i].rtflags.SKIP = 0;

    outFlag = OUTOK;
    ScrNewUser();
    if (exitValue != DOOR_EXIT)
	getRoom(LOBBY);

    ClearDoorTimers();

    IgMailCleanup();
}

