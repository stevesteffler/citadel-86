/************************************************************************/
/*				Stroll.C				*/
/*                      handles Stroll for Citadel-86                   */
/************************************************************************/

#define NO_STROLL

#ifndef NO_STROLL

#include "ctdl.h"

/************************************************************************/
/*                              history					*/
/*									*/
/* 89Aug15 HAW  Created.						*/
/************************************************************************/


/************************************************************************/
/*                              Contents                                */
/*                                                                      */
/*	StrollIt()		Handles a stroll call.			*/
/************************************************************************/


/*
 This may need to be moved to a header file in the future.
 */
#define TERMINATE	199
#define SendBanner	88
#define SendRoomlist	193
#define SendBlurb	89
#define DoGotoRoom	101
#define NewMsgsStroll	80
#define ReadForward	81 
#define ReadReverse	82 
#define ReadOldRev	83 
#define DirStroll	84
#define ExtDirStroll	85
#define StrollStatus	86
 
extern CONFIG      cfg;            /* Lots an lots of variables    */
extern aRoom       roomBuf;        /* Room buffer                        */
extern logBuffer   logBuf;         /* Log buffer of a person       */
extern AN_UNSIGNED RecBuf[];
extern struct floor  *FloorTab;
extern rTable      *roomTab;       /* RAM index of rooms        */
extern char        inNet;
extern char	   loggedIn;
extern int	   thisRoom;
extern char        onConsole;      /* Flag                         */
extern char        remoteSysop;

void StrollMainLoop(void);
void BannerStroll(void);
void NoticeStroll(void);
void GotoRoomStroll(struct cmd_data *cmds);
void LogOutStroll(void);
int StrollRoomInfo(int i);
void SendStrollDir(struct cmd_data *cmds);
void StrollReply(struct cmd_data *cmds);
void SendStatus(void);
char StrollValidate(int mode);
void SendStrollMsgs(int mode, int revOrder, struct cmd_data *cmds);
void KnownRoomsStroll(void);

FILE *strollfd = NULL;
/************************************************************************/
/*	StrollIt() Handles a stroll call.				*/
/************************************************************************/
void StrollIt()
{
    char *User, *Pwd;
    extern void (*NetPrintTarget)();

    NetPrintTarget = mTrPrintf;
    inNet = STROLL_CALL;
strollfd = fopen("strltest\\report", "w");
    fprintf(strollfd, "In StrollIt()!\n");
    if (called_stabilize()) {
fprintf(strollfd, "Stroll call\n");
	printf("Stroll Call stabilized!\n");
	ITL_InitCall();             /* Initialize the ITL layer */
	ITL_Receive(NULL, FALSE, TRUE, putFLChar, fclose);

	if (gotCarrier()) {
	    User = Pwd = RecBuf;
	    while (*Pwd)
	        Pwd++;
	    Pwd++;

	    if (PWSlot(Pwd, TRUE) != ERROR)
	        if (strCmpU(User, logBuf.lbname) == SAMESTRING)
		    StrollMainLoop();

	    pause(100);
	    killConnection("stroll");
	}

	ITL_DeInit();
    }
fclose(strollfd);
strollfd = NULL;
    inNet = NON_NET;
}

/************************************************************************/
/*	StrollMainLoop() stroll caller's main loop.			*/
/************************************************************************/
static void StrollMainLoop()
{
    struct cmd_data cmds;
    label           tempNm;

fprintf(strollfd, "Logged in as %s.\n", logBuf.lbname);
    loggedIn     = TRUE;
    setUp(TRUE);

    do {
        getNextCommand(&cmds);
fprintf(strollfd, "Requesting command %d.\n", cmds.command);
        switch (cmds.command) {
	    case TERMINATE:
		LogOutStroll();
		break;
	    case SendBanner:
		BannerStroll();
		break;
	    case SendBlurb:
		NoticeStroll();
		break;
	    case SendRoomlist:
		KnownRoomsStroll();
		break;
	    case DoGotoRoom:
		GotoRoomStroll(&cmds);
		break;
	    case NewMsgsStroll:
		SendStrollMsgs(NEWoNLY, FALSE, &cmds);
		break;
	    case ReadForward:
		SendStrollMsgs(OLDaNDnEW, FALSE, &cmds);
		break;
	    case ReadReverse:
		SendStrollMsgs(OLDaNDnEW, TRUE, &cmds);
		break;
	    case ReadOldRev:
		SendStrollMsgs(OLDoNLY, TRUE, &cmds);
		break;
	    case DirStroll:
	    case ExtDirStroll:
		SendStrollDir(&cmds);
		break;
	    case StrollStatus:
		SendStatus();
		break;
            default:
		sprintf(tempNm, "'%d' unknown.", cmds.command);
                reply(BAD, tempNm);
		break;
	}
    } while (gotCarrier() && cmds.command != TERMINATE);
    loggedIn = FALSE;
}

/************************************************************************/
/*	LogOutStroll() log out on stroll.				*/
/************************************************************************/
void LogOutStroll()
{
    loggedIn     = FALSE;
    reply(GOOD, "");
    if (ITL_Send(STARTUP)) {
	if (!printHelp("lonotice", HELP_USE_BANNERS))
	    printHelp("lonotice.blb", 0);
        ITL_Send(FINISH);
    }
}

/************************************************************************/
/*	NoticeStroll() send the notice.					*/
/************************************************************************/
void NoticeStroll()
{
    reply(GOOD, "");
    if (ITL_Send(STARTUP)) {
	if (!printHelp("notice", HELP_USE_BANNERS))
	    printHelp("notice.blb", 0);
        ITL_Send(FINISH);
    }
}

/************************************************************************/
/*	BannerStroll() send the notice.					*/
/************************************************************************/
void BannerStroll()
{
    extern char *VERSION;

    reply(GOOD, "");
    if (ITL_Send(STARTUP)) {
	if (!printHelp("banner", HELP_USE_BANNERS))
	    if (!printHelp("banner.blb", 0))
		mPrintf("Welcome to %s\n", cfg.codeBuf + cfg.nodeTitle);
	mPrintf(" Running: %s (V%s) \n  ", VARIANT_NAME, VERSION);
	mPrintf(formDate());
        ITL_Send(FINISH);
    }
}

/************************************************************************/
/*	KnownRoomsStroll() send the known rooms list			*/
/************************************************************************/
void KnownRoomsStroll()
{
    reply(GOOD, "");
    if (ITL_Send(STARTUP)) {
	tableRunner(StrollRoomInfo, TRUE);
	sendITLchar(ETX);
        ITL_Send(FINISH);
    }
}

/************************************************************************/
/*	StrollRoomInfo() sends room info via STROLL.			*/
/************************************************************************/
int StrollRoomInfo(int i)
{
    char one, two;
    /*
     * A = '>'
     * B = ')'
     * C = ']'
     * D = ':'
     *
     * 1 = Room has new messages
     * 0 = Room has no new messages
     */
    static char matrix[2][2] =
      {  { 'A', 'B' } ,
         { 'C', 'D' } } ;

    one = roomTab[i].rtflags.ISDIR;
    two = (roomTab[i].rtflags.SHARED && cfg.BoolFlags.netParticipant);
    mTrPrintf("%s", roomTab[i].rtname);
    mTrPrintf("%s", FloorTab[roomTab[i].rtFlIndex].FlName);
    mTrPrintf("%c%c", matrix[one][two], RoomHasNew(i) ? '1' : '0');
    return FALSE;
}

/************************************************************************/
/*	GotoRoomStroll() goto a room via stroll.			*/
/************************************************************************/
void GotoRoomStroll(struct cmd_data *cmds)
{
    int count, NewCount;

    if (GotoNamedRoom(cmds->fields[0], 'G') == ERROR)
	reply(BAD, "No such room");
    else {
	cmds->command = GOOD;
	CountMsgs(&count, &NewCount);
	sprintf(cmds->fields[0], "%d", count);
	sprintf(cmds->fields[1], "%d", NewCount);
	StrollReply(cmds);
    }
}

/************************************************************************/
/*      StrollReply() Replies to caller					*/
/************************************************************************/
void StrollReply(struct cmd_data *cmds)
{
    int count;

    if (!ITL_Send(STARTUP)) {
        no_good("Couldn't send reply to %s!", TRUE);
        return;
    }
    sendITLchar(cmds->command);
    for (count = 0; count < 4; count++) {
        if (cmds->fields[count][0]) {
            mTrPrintf("%s", cmds->fields[count]);
        }
    }
    sendITLchar(0);
    ITL_Send(FINISH);
}

/************************************************************************/
/*	SendStrollMsgs() send user's messages.				*/
/************************************************************************/
void SendStrollMsgs(int mode, int revOrder, struct cmd_data *cmds)
{
    extern OptValues Opt;

    reply(GOOD, "");
    if (ITL_SendMessages()) {
	zero_struct(Opt);
	if (strCmp(cmds->fields[0], " ") != SAMESTRING) {
            if (ReadDate(cmds->fields[0], &Opt.Date) == ERROR)
		Opt.Date = -1l;
        }
	else Opt.Date = -1l;
	if (strCmp(cmds->fields[1], " ") != SAMESTRING)
	    strCpy(Opt.User, cmds->fields[1]);
	if (strCmp(cmds->fields[2], " ") != SAMESTRING)
	    strCpy(Opt.Phrase, cmds->fields[2]);
	showMessages(mode, revOrder, logBuf.lastvisit[thisRoom],StrollValidate);
	ITL_StopSendMessages();
    }
}

/************************************************************************/
/*	StrollValidate() should we send this message?			*/
/************************************************************************/
char StrollValidate(int mode)
{
    if (OptionCheck(mode))
        prNetStyle(0, sendITLchar, TRUE, "");

    return TRUE;
}

/************************************************************************/
/*	SendStrollDir() send a directory via stroll.			*/
/************************************************************************/
void SendStrollDir(struct cmd_data *cmds)
{
    char Spec[50];

fprintf(strollfd, "In StrollDir.\n");
    if (roomBuf.rbflags.ISDIR == 1 &&
				(roomBuf.rbflags.DOWNLOAD || SomeSysop())) {
fprintf(strollfd, "StrollDir is replying GOOD.\n");
	reply(GOOD, "");
fprintf(strollfd, "field0 is -%s-\nfield1 is -%s-\nfield2 is -%s-\nfield3 is -%s-.\n", cmds->fields[0], cmds->fields[1], cmds->fields[2], cmds->fields[3]);
	sprintf(Spec, "%s %s", cmds->fields[0], cmds->fields[1]);
	if (ITL_Send(STARTUP)) {
	    doDirectory((cmds->command == DirStroll), Spec, cmds->fields[2]);
	    sendITLchar(ETX);
            ITL_Send(FINISH);
        }
    }
    else {
	fprintf(strollfd, "StrollDir is replying BAD.\n");
	reply(BAD, "No directory");
    }
}

/************************************************************************/
/*	SendStatus() send .RS via stroll.				*/
/************************************************************************/
void SendStatus()
{
    reply(GOOD, "");
    if (ITL_Send(STARTUP)) {
	systat();
	sendITLchar(ETX);
        ITL_Send(FINISH);
    }
}
#else

void StrollIt()
{
}

#endif

