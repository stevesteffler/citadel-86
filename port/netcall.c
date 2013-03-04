/*
 *				netcall.c
 *
 * Networking call functions.
 */

/*
 *				history
 *
 * 86Aug20 HAW  History not maintained due to space problems.
 */
#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"

/*
 *				contents
 *
 */

char			inReceive;
NetInfo			NetStyle;

extern char		RecMassTransfer;
extern SListBase	SystemsCalled;
extern FILE		*netMisc;
extern FILE		*netLog;
extern AN_UNSIGNED      RecBuf[SECTSIZE + 5];
extern int		counter;
extern int		callSlot;
extern label		callerName, callerId;
char			checkNegMail;
extern char		processMail;
extern char		MassTransferSent;

char		normId(), getNetMessage();

extern CONFIG    cfg;		/* Lots an lots of variables    */
extern logBuffer logBuf;		/* Person buffer		*/
extern logBuffer logTmp;		/* Person buffer		*/
extern aRoom     roomBuf;		/* Room buffer		*/
extern rTable    *roomTab;
extern PROTO_TABLE Table[];
extern int TransProtocol;
extern MessageBuffer   msgBuf;
extern NetBuffer netBuf, netTemp;
extern NetTable  *netTab;
/* extern SListBase FwdAliasii; */
extern int       thisNet;
extern char      onConsole;
extern char      loggedIn;       /* Is we logged in?		*/
extern char      outFlag;		/* Output flag		*/
extern char      haveCarrier;    /* Do we still got carrier?     */
extern char      modStat;		/* Needed so we don't die       */
extern char      TrError;
extern int       thisRoom;
extern char      netDebug;
extern char	 *DomainFlags;
extern long ByteCount, EncCount;
extern char *R_SH_MARK, *NON_LOC_NET, *LOC_NET;
extern char *READ_ANY;

char *chMailTemplate = "chkMail.$$$";

/*
 * caller()
 *
 * This function is called when we've been called and have to handle the
 * caller.  We have to stabilize the call and then manage all requests
 * the caller makes of us.
 */
char caller()
{
    ITL_InitCall();		/* initialize the ITL layer */
    RecMassTransfer = FALSE;

    inReceive = FALSE;
    processMail = FALSE;
    checkNegMail = FALSE;

    splitF(netLog, "Have Carrier\n");

    caller_stabilize();
    if (!haveCarrier) return FALSE;  /* Abort */

    splitF(netLog, "Stabilized\n");

    sendId();
    if (!haveCarrier) return TRUE;  /* Abort */

    if (!netBuf.nbflags.MassTransfer)
	ITL_optimize(TRUE);		/* try for better protocol */

    sendStuff(FALSE, TRUE, TRUE);

    startTimer(WORK_TIMER);
    while (gotCarrier() && chkTimeSince(WORK_TIMER) < 10) ;

    killConnection("caller");

    splitF(netLog, "Finished with %s @%s\n", netBuf.netName, Current_Time());

    doResults();
    return TRUE;
}

/*
 * sendStuff()
 *
 * This function handles being the sender of information (sending role).
 */
void sendStuff(char reversed, char SureDoIt, char SendRooms)
{
	SystemCallRecord *called;
	void SendOtherRoutedMail();
	extern int RouteToDirect;
	extern SListBase Routes;

	called = SearchList(&SystemsCalled, &thisNet);

	if (SureDoIt && callSlot != ERROR) {
		MassTransferSent = FALSE;
		SendPwd();
		if (!haveCarrier) return ;  /* Abort */

		if (netBuf.nbflags.HasRouted)
			RouteOut(&netBuf, thisNet, TRUE);

		RunListA(&Routes, SendOtherRoutedMail, &thisNet);

		if (!haveCarrier) return ;  /* Abort */

		DomainOut(FALSE);

		if (!haveCarrier) return ;  /* Abort */

		if (netBuf.nbflags.normal_mail ||
			RouteToDirect != -1 ||
		  AnyRouted(thisNet)
				) {
			sendMail();
			if (!haveCarrier) return ;  /* Abort */
			checkMail();
		}
		if (!haveCarrier) return ;  /* Abort */

		if (!HasPriorityMail(thisNet)) {
			if (netBuf.nbflags.room_files)
				askFiles();
			if (!haveCarrier) return ;  /* Abort */

			if (SendRooms) sendSharedRooms();
			if (!haveCarrier) {
				called->Status = SYSTEM_INTERRUPTED;
				return ;  /* Abort */
			}

			if (netBuf.nbflags.send_files)
				doSendFiles();
			if (!haveCarrier) return ;  /* Abort */

			roleReversal(reversed,
					(called->Status == SYSTEM_INTERRUPTED));
		}
	}

	sendHangUp();

	/*
	 * We only get here on a successful call
	 */
	if ((called = SearchList(&SystemsCalled, &thisNet)) != NULL)
		called->Status = SYSTEM_CALLED;
}

/*
 * SendPwd()
 *
 * This function sends the system password if necessary.
 */
void SendPwd()
{
    struct cmd_data cmds;

    if (netBuf.TheirPwd[0] != 0) {      /* only send if need to -- gets */
	zero_struct(cmds);		/* us around a bug in pre net 1.10*/
	strcpy(cmds.fields[0], netBuf.TheirPwd);	/* versions     */
	cmds.command = SYS_NET_PWD;
	sendNetCommand(&cmds, "system pwd");
    }
}

/*
 * roleReversal()
 *
 * This function handles the role reversal request.
 */
void roleReversal(char reversed, int interrupted)
{
    struct cmd_data cmds;

    if (reversed) return ;
    if (!netBuf.nbflags.local && !netBuf.nbflags.spine) return ;

    splitF(netLog, "Reversing roles @ %s\n", Current_Time());

    zero_struct(cmds);
    cmds.command = ROLE_REVERSAL;
    if (interrupted)
	strcpy(cmds.fields[0], "interrupted");

    if (!sendNetCommand(&cmds, "role reversal"))
	return;

    rcvStuff(TRUE);
    pause(100);		/* wait a second */

    if (gotCarrier())
	reply(GOOD, "");/* this replies GOOD to the HANGUP terminating the */
			/* role reversal.  NOTE: STadel doesn't follow the */
			/* spec in this regard - it just dumps carrier	   */
}

/*
 * caller_stabilize()
 *
 * This function tries to stabilize the call -- baud is already set.
 */
void caller_stabilize()
{
    int tries, x1, x2, x3;
    extern char hst;
    void Clean();
    extern SListBase SharedRooms;

		/* regrettable initialization */
    RunList(&SharedRooms, Clean);

    while (MIReady())   Citinp();		/* Clear garbage */

    if (netBuf.nbflags.Login) {	/* if so, wait for room prompt */
	pause(200);
	startTimer(USER_TIMER);		/* this is safe */
	while (!MIReady() && chkTimeSince(USER_TIMER) < 4l && gotCarrier())
	    ;
	while (gotCarrier() && receive(3) != -1)
	    ;
    }

    startTimer(USER_TIMER);		/* this is safe */
    for (tries = 0; (chkTimeSince(USER_TIMER) < 10l || tries < 20) &&
						gotCarrier() ; tries++) {
	if (cfg.BoolFlags.debug) splitF(netLog, ".");
	outMod(7);
	outMod(13);
	outMod(69);
	for (startTimer(WORK_TIMER); chkTimeSince(WORK_TIMER) < 2l &&
								!MIReady();)
	    ;

	if (MIReady()) {
	    x1 = receive(2);
	    x2 = receive(2);
	    if (x2 != ERROR) x3 = receive(2);
	    if (x1 == 248 &&
		x2 == 242 &&
		x3 == 186) {
		outMod(ACK);
		if (cfg.BoolFlags.debug)
		    splitF(netLog, "ACKing, Call stabilized\n");

		/* ok, we've seen at high speed some overrun problems, so ... */
		do
		    x1 = receive(2);
		while (x1 == 248 || x1 == 242 || x1 == 186);
		ModemPushBack(x1);

		return;
	    }
	    else {
		if (cfg.BoolFlags.debug)
		    splitF(netLog, "%d %d %d\n", x1, x2, x3);
		if ((x1 == 242 || x1 == 186) && (x2 == 248 || x2 == 186)) {
			/* real close, so let's catch our breath */
		    if (cfg.BoolFlags.debug)
			splitF(netLog, "ReSynch\n");
		    while (receive(1) != ERROR)
			;
		}
	    }
	}
    }
    splitF(netLog, "Call not stabilized\n");
    killConnection("caller_stab");
}

/*
 * sendId()
 *
 * This function sends ID to the receiver.
 */
void sendId()
{
    if (!ITL_Send(STARTUP)) {
	no_good("Couldn't transfer ID to %s!", TRUE);
	return;
    }

    NetPrintf(sendITLchar, "%s", cfg.codeBuf + cfg.nodeId  );
    NetPrintf(sendITLchar, "%s", cfg.codeBuf + cfg.nodeName);

    if (!ITL_Send(FINISH)) {
	no_good("Couldn't transfer ID to %s!", TRUE);
	return;
    }
}

/*
 * sendMail()
 *
 * This function sends normal mail to receiver.
 */
void sendMail()
{
    extern SListBase Routes;
    struct cmd_data cmds;
    int		nor_mail = 0;
    extern int      RouteToDirect;

    if (!gotCarrier()) {
	modStat = haveCarrier = FALSE;
	return ;
    }

    splitF(netLog, "Sending Mail ");

    zero_struct(cmds);
    cmds.command = NORMAL_MAIL;
    if (!sendNetCommand(&cmds, "normal mail"))
	return;

    if (!ITL_SendMessages()) {
	no_good("Couldn't start Mail transfer to %s!", TRUE);
	killConnection("sendmail");
	return;
    }

    if (netBuf.nbflags.normal_mail)
	nor_mail = send_direct_mail(thisNet, netBuf.netName);

    if (gotCarrier())
	nor_mail += SendRoutedAsLocal();

    ITL_StopSendMessages();

    RouteToDirect = -1;		/* This is just in case */
    if (gotCarrier()) {
	splitF(netLog, "(%d) (%ld => %ld bytes)\n",nor_mail,EncCount,ByteCount);
	netBuf.nbflags.normal_mail = FALSE;
    }
}

/*
 * checkMail()
 *
 * This function handles negative acknowledgement on netMail.
 */
void checkMail()
{
    struct cmd_data cmds;
    SYS_FILE fileNm;
    extern char *WRITE_ANY;

    if (!gotCarrier()) {
	return;
    }

    splitF(netLog, "Check mail\n");

    makeSysName(fileNm, chMailTemplate, &cfg.netArea);

    zero_struct(cmds);
    cmds.command = CHECK_MAIL;
    if (!sendNetCommand(&cmds, "check mail")) {
	return;
    }

    if (ITL_Receive(fileNm, FALSE, TRUE, putFLChar, fclose) == ITL_SUCCESS)
	checkNegMail = TRUE;		/* Call readNegMail() later */
}

/*
 * readNegMail()
 *
 * This function reads and processes negative acks.
 */
void readNegMail(char talk)
{
    label author, target, context;
    int whatLog;
    int  sigChar;
    SYS_FILE fileNm;

    makeSysName(fileNm, chMailTemplate, &cfg.netArea);
    if ((netMisc = fopen(fileNm, READ_ANY)) == NULL) {
	if (talk) no_good("Couldn't open negative ack file from %s.", FALSE);
	return ;
    }

    getRoom(MAILROOM);
    sigChar = fgetc(netMisc);
    while (sigChar != NO_ERROR && sigChar != EOF) {
	ZeroMsgBuffer(&msgBuf);
	strcpy(msgBuf.mbauth, "Citadel");
	getMsgStr(getNetChar, author, NAMESIZE);
	getMsgStr(getNetChar, target, NAMESIZE);
	getMsgStr(getNetChar, context, NAMESIZE);
	switch (sigChar) {
	    case NO_RECIPIENT:
		strcpy(msgBuf.mbto, author);
		if ((whatLog = PersonExists(author)) >= 0 &&
						whatLog < cfg.MAXLOGTAB) {
		    sprintf(msgBuf.mbtext,
"Your netMail to '%s' (%s) failed because there is no such recipient on %s.",
		    target, context, callerName);
		    putMessage(&logBuf, SKIP_AUTHOR);
		    break;
		}
	    case UNKNOWN:
		ZeroMsgBuffer(&msgBuf);
		sprintf(msgBuf.mbtext,
			"Mail Check: Unknown problems with netMail: author=-%s-, target=-%s-, context=-%s-.  System was %s.",
				author, target, context, netBuf.netName);
		netResult(msgBuf.mbtext);
		break;
	    case BAD_FORM:
		sprintf(msgBuf.mbtext, "Mail Check: Bad netMail sent to %s.", callerName);
		netResult(msgBuf.mbtext);
		break;
	    default:
		sprintf(msgBuf.mbtext, "Mail Check: Bad sigChar=%d.", sigChar);
		netResult(msgBuf.mbtext);
		break;
	}
	sigChar = fgetc(netMisc);
    }
    fclose(netMisc);
    unlink(fileNm);
}

/*
 * sendSharedRooms()
 *
 * This sends all shared rooms to receiver.
 */
static void sendSharedRooms()
{
	int SendRoom(SharedRoomData *room, int system, int roomslot, void *d);
	SystemCallRecord *called;

	if ((called = SearchList(&SystemsCalled, &thisNet)) == NULL) {
		splitF(netLog, "Internal bug - can't find %d in SystemsCalled?!\n", thisNet);
		return;
	}

	SendFastTransfer();

	EachSharedRoom(thisNet, SendRoom, SendVirtualRoom, called);
}

/*
 * Addressing()
 *
 * This function is responsible for deciding what sort of addressing or routing
 * flags should be checked for, and if the room should be sent if we are in
 * a network session.
 */
void Addressing(int system, SharedRoom *room, char *commnd, char **send1,
			char **send2, char **send3, char **name, char *doit)
{

    /*
     * This is more than just a trivial efficiency.  This routine can be called
     * indirectly by the room editing functions.  If this happens then the
     * getRoom() call would overwrite the roomBuf being used for editing.
     * Therefore, we can only read in roomBuf if we don't have the right one
     * in place, otherwise we lose what we just changed.
     */
    if (thisRoom != netRoomSlot(room))
	getRoom(netRoomSlot(room));

    *doit = TRUE;
    *send1 = R_SH_MARK;
    *send2 = *send3 = "guh";
    switch (GetMode(room->mode)) {
    case PEON:
	*commnd = NET_ROOM;
	*send2 = NON_LOC_NET;
	*name = "Peon";
	break;
    case BACKBONE:
	if (netTab[system].ntflags.local)
	    *commnd = NET_ROOM;
	else
	    *commnd = NET_ROUTE_ROOM;
	*send2 = NON_LOC_NET;
	*send3 = LOC_NET;
	*name = "Backbone";
	break;
    default: crashout("shared rooms: #2");
    }
}

/*
 * SendRoom()
 *
 * Sends a room to the receiving system during netting.  It returns ERROR if
 * carrier etc is lost.
 */
static int SendRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
	char cmd;
	char doit, *s1, *s2, *s3, *name;
	SystemCallRecord *called;
	int *slot;

	called = d;
	if (!gotCarrier()) {
		modStat = haveCarrier = FALSE;
		return ERROR;
	}

	if (SearchList(&called->SentRooms, &roomslot) != NULL) {
		return TRUE;	/* already sent, perhaps in another connect */
	}

	Addressing(system, room->room, &cmd, &s1, &s2, &s3, &name, &doit);
	if (doit && !(room->room->sr_flags & SR_SENT) &&
		(roomTab[roomslot].rtlastNetAll >
			room->room->lastMess ||
		(roomTab[roomslot].rtlastNetBB >
			room->room->lastMess &&
		GetMode(room->room->mode) == BACKBONE) ||
		GetFA(room->room->mode) ||
		(GetMode(room->room->mode) != PEON && !netBuf.nbflags.local))
	) {
		ITL_optimize(TRUE);
		if (findAndSend((RecMassTransfer ||
			(room->room->sr_flags & SR_RECEIVED)) ?
			NET_ROOM : cmd, room,
			RoomSend, roomTab[roomslot].rtname, RoomReceive)) {
			/* if successful, record event */
			slot = GetDynamic(sizeof *slot);
			(*slot) = roomslot;
			AddData(&called->SentRooms, slot, NULL, FALSE);
		}
	}

	return TRUE;
}

/*
 * findAndSend()
 *
 * This function manages sending a room (virtual or normal) to the receiver,
 * handling both normal and route rooms, via function pointers.
 *
 * If operation(s) successful, return TRUE, otherwise FALSE.
 */
int findAndSend(int commnd, SharedRoomData *room,
		int (*MsgSender)(SharedRoomData *r), label roomName,
		int (*MsgReceiver)(SharedRoomData *r, char y))
{
    struct cmd_data cmds;
    extern MessageBuffer tempMess;
    int  tempcount;
    extern char *netRoomTemplate, *WRITE_ANY;

    if (!gotCarrier()) return FALSE;

    zero_struct(cmds);
    cmds.command = commnd;
    strcpy(cmds.fields[0], roomName);
    if (commnd != ERROR)
	if (!sendNetCommand(&cmds, "shared rooms")) {
	    if (commnd == NET_ROUTE_ROOM) {
							/* time to recurse */
		return findAndSend(NET_ROOM, room, MsgSender, roomName,
							MsgReceiver);
	    }
	    else {
		sprintf(tempMess.mbtext, "%%s reports: %s (%s)", RecBuf+1,
								roomName);
	    	no_good(tempMess.mbtext, FALSE);
	    	return FALSE;
	    }
	}

    if (!ITL_SendMessages()) {
	no_good("Couldn't start transfer for room sharing: %s", FALSE);
	return FALSE;
    }
    tempcount = (*MsgSender)(room);
    ITL_StopSendMessages();
    splitF(netLog, "(%d) (%ld => %ld bytes)\n", tempcount, EncCount, ByteCount);
    if (commnd == NET_ROUTE_ROOM) {
	(*MsgReceiver)(room, FALSE);
    }
    return (gotCarrier()) ? TRUE : FALSE;
}

/*
 * RoomReceive()
 *
 * This function receives messages for a room.
 */
int RoomReceive(SharedRoomData *room, char ReplyFirst)
{
    recNetMessages(room->room, roomBuf.rbname, netRoomSlot(room->room), FALSE);
    return 0;
}

/*
 * RoomSend()
 *
 * This function sends messages for a room.
 */
int RoomSend(SharedRoomData *room)
{
    extern char PrTransmit;
    char work[10];
    int MsgCount = 0;
    char cmd;
    char doit, *name;

    splitF(netLog, "Sending %s ", roomBuf.rbname);

    zero_struct(NetStyle);

    Addressing(thisNet, room->room, &cmd, &NetStyle.addr1, &NetStyle.addr2,
				&NetStyle.addr3, &name, &doit);

    if (GetFA(room->room->mode)) {
	sprintf(work, CACHE_END_NAME, netRoomSlot(room->room));
	if (SendPrepAsNormal(work, &MsgCount))
	    UnSetFA(room->room->mode);
    }

    NetStyle.sendfunc = sendITLchar;
    MsgCount += showMessages(NEWoNLY, room->room->lastMess, NetRoute);

    if (TrError == TRAN_SUCCESS) {
	SetHighValues(room);
	room->room->sr_flags |= SR_SENT;
    }
    else {
	MsgCount = 0;
    }

    return MsgCount;
}

/*
 * SetHighValues()
 *
 * This function sets the high message sent for a normal shared room after a
 * successful net session has (apparently) taken place.
 */
void SetHighValues(SharedRoomData *room)
{
    if (NetStyle.HiSent == 0l) {
	NetStyle.HiSent = cfg.newest;
    }
    room->room->lastMess = NetStyle.HiSent;
    room->srd_flags |= SRD_DIRTY;
}

/*
 * SendPrepAsNormal()
 *
 * This function sends files prepared for cache sending as normal message files,
 * instead.
 */
char SendPrepAsNormal(char *work, int *MsgCount)
{
    char tempNm[3*NAMESIZE];

    NetCacheName(tempNm, thisNet, work);
    if ((netMisc = fopen(tempNm, READ_ANY)) != NULL) {
	while (getMessage(getNetChar, TRUE, TRUE, TRUE)) {
	    (*MsgCount)++;
	    prNetStyle(FALSE, getNetChar, sendITLchar, FALSE, "");
	}
	fclose(netMisc);
    }
    if (gotCarrier()) {
	unlink(tempNm);
	return TRUE;
    }
    return FALSE;
}

/*
 * NetRoute()
 *
 * This is a worker function, returns TRUE if message sent.
 */
char NetRoute(int status, int msgslot)
{
    if ((strncmp(msgBuf.mbaddr, NetStyle.addr1, strLen(NetStyle.addr1))
						== SAMESTRING  ||
	 strncmp(msgBuf.mbaddr, NetStyle.addr2, strLen(NetStyle.addr2))
						== SAMESTRING  ||
	 strncmp(msgBuf.mbaddr, NetStyle.addr3, strLen(NetStyle.addr3))
						== SAMESTRING) &&
	 RoutePath(LOC_NET, msgBuf.mbaddr)     != thisNet       &&
	 RoutePath(NON_LOC_NET, msgBuf.mbaddr) != thisNet) {
        msgBuf.mbaddr[0] = 0;	/* 120.678 */
	prNetStyle(FALSE, getMsgChar, NetStyle.sendfunc, TRUE, "");
	NetStyle.HiSent = max(NetStyle.HiSent, atol(msgBuf.mbId));
	return TRUE;
    }
    return FALSE;
}

/*
 * RoutePath()
 *
 * This function returns the number of the node that routed this msg to here.
 * If the msg was not routed in from a BackBone, then return ERROR, which will
 * never match another node's #.
 *
 * 88Oct13: Now simply check for msg origin, assume if one exists that it
 * should be checked.  Don't remember why it is restricted to only
 * BACKBONE-routed msgs.  Doesn't seem necessary.
 */
int RoutePath(char *rp, char *str)
{
    if (strncmp(rp, str, strLen(rp)) == SAMESTRING) {
	if (strLen(str) != strLen(rp)) /* prevent return of 0 */
	    return atoi(str + 2);
    }
    return ERROR;
}

/*
 * doSendFiles()
 *
 * This function will send files to a victim.
 */
void doSendFiles()
{
    extern char       *READ_ANY;
    struct   fl_send  theFiles;
    SYS_FILE	      sdFile;
    char	      temp[8];
    FILE	      *fd;

    ITL_optimize(FALSE);		/* try for better dft protocol */
    sprintf(temp, "%d.sfl", thisNet);
    makeSysName(sdFile, temp, &cfg.netArea);
    if ((fd = fopen(sdFile, READ_ANY)) == NULL) {
	sprintf(msgBuf.mbtext, "Send File: Couldn't open send file for %s.",
						netBuf.netName);
	netResult(msgBuf.mbtext);
	netBuf.nbflags.send_files = FALSE;
    }
    else {
	while (getSLNet(theFiles, fd) && haveCarrier) {
	    sysSendFiles(&theFiles);
	}
	fclose(fd);
	if (haveCarrier) {  /* if no carrier, was an error during transmit */
	    unlink(sdFile);
	    netBuf.nbflags.send_files = FALSE;
	}
    }

}

/*
 * netSendFile()
 *
 * This function will send a file to another system via net.
 */
void netSendFile(DirEntry *fn)
{
	void SendFileResults(char *name, char *node);
	PROTOCOL		*External;
	struct cmd_data	cmds;
	char		mess[140];

	if (!gotCarrier()) return ;

	splitF(netLog, "Send File: %s\n", fn->unambig);
	zero_struct(cmds);
	strcpy(cmds.fields[0], fn->unambig);
	sprintf(cmds.fields[1], "%ld",
			(fn->FileSize + SECTSIZE - 1) / SECTSIZE);
	sprintf(cmds.fields[2], "%ld", fn->FileSize);

	/* first try the send file with protocol selection - zmodem only */
	if ((External = FindProtocolByName("Zmodem", FALSE)) != NULL) {
		cmds.command = SF_PROTO;
		sprintf(cmds.fields[3], "%ld", ZM_PROTOCOL);
		if (sendNetCommand(&cmds, "send file wp")) {
			ExternalTransfer(External, fn->unambig);
			SendFileResults(fn->unambig, netBuf.netName);
			while (MIReady())   Citinp();	/* Clear garbage */
			return;
		}
	}
	cmds.command = SEND_FILE;

	if (!sendNetCommand(&cmds, "send file")) {
		if (haveCarrier) {
			strcpy(mess, "%s reports: ");
			strcat(mess, RecBuf + 1);
			no_good(mess, FALSE);
		}
	}
	else {
		SendHostFile(fn->unambig);
		SendFileResults(fn->unambig, netBuf.netName);
	}
}

static void SendFileResults(char *name, char *node)
{
	if (haveCarrier) {
		sprintf(msgBuf.mbtext, "Send File: %s sent to %s.", name, node);
		netResult(msgBuf.mbtext);
	}
}

extern FILE *upfd;
/*
 * askFiles()
 *
 * This function will ask for file(s) from caller.
 */
void askFiles()
{
    label    data2;
    void fl_req_free();
    SYS_FILE dataFl;
    char     mess[130];
    char     ambiguous;
    int      result = ITL_SUCCESS;
    FILE     *temp;
    struct   cmd_data cmds;
    struct   fl_req file_data, *list;
    SListBase Failed = { NULL, NULL, NULL, fl_req_free, NULL };
    extern char *READ_ANY, *WRITE_ANY;

    if (!gotCarrier()) {
	modStat = haveCarrier = FALSE;
	return ;
    }

    sprintf(data2, "%d.rfl", thisNet);
    makeSysName(dataFl, data2, &cfg.netArea);
    temp = fopen(dataFl, READ_ANY);
    if (temp == NULL) {
	no_good("Couldn't open room request file for %s", FALSE);
	netBuf.nbflags.room_files = FALSE;
    }
    else {
	ITL_optimize(FALSE);		/* try for better protocol */
	while ( result == ITL_SUCCESS &&
		fread(&file_data, sizeof (file_data), 1, temp) == 1 &&
		gotCarrier() && result == ITL_SUCCESS) {
	    if (netSetNewArea(&file_data.flArea)) {
		zero_struct(cmds);
		ambiguous = !(strchr(file_data.roomfile, '*') == NULL &&
				strchr(file_data.roomfile, '?') == NULL);
		cmds.command = (!ambiguous) ? R_FILE_REQ : A_FILE_REQ;

		strcpy(cmds.fields[0], file_data.room);
		strcpy(cmds.fields[1], file_data.roomfile);
		splitF(netLog, "Requesting %s in %s\n", file_data.roomfile, 
					file_data.room);
		if (!sendNetCommand(&cmds,
		(!ambiguous) ? "single file request" :
					"multiple file request")) {
splitF(netLog, "sNC failure\n");
		    sprintf(mess, "%%s reports %s for file %s in %s.", RecBuf+1,
					file_data.roomfile, file_data.room);
		    no_good(mess, FALSE);
		}
		else {
splitF(netLog, "sNC success ambiguous is %d\n", ambiguous);
		    if (ambiguous)
			result = multiReceive(&file_data);
		    else {
			if ((result = ITL_Receive(file_data.filename, FALSE,
				TRUE, putFLChar, fclose)) == ITL_SUCCESS) {
			    sprintf(msgBuf.mbtext,
			"Request File: %s received from %s (stored in %s).",
					file_data.filename, netBuf.netName,
					prtNetArea(&file_data.flArea));
			    netResult(msgBuf.mbtext);
			}
		    }
		}
	    }
	    homeSpace();
	}
	if (gotCarrier()) {
	    fclose(temp);
	    unlink(dataFl);
	    netBuf.nbflags.room_files = FALSE;
	}
	else {
	    haveCarrier = modStat = FALSE;
	/* Now find out what we didn't get and set up a new request queue */
	    do {	/* use do loop to get the one it failed in */
		list = GetDynamic(sizeof *list);
		copy_struct(file_data, (*list));
		AddData(&Failed, list, NULL, FALSE);
	    } while (fread(&file_data, sizeof (file_data), 1, temp) == 1);
	    fclose(temp);
	    unlink(dataFl);
	    upfd = fopen(dataFl, WRITE_ANY);
	    KillList(&Failed);
	    fclose(upfd);
	}
    }
}

/*
 * fl_req_free()
 *
 * This will write and free a file request.
 */
static void fl_req_free(struct fl_req *d)
{
    if (upfd != NULL) fwrite(d, sizeof *d, 1, upfd);
    free(d);
}

/*
 * multiReceive()
 *
 * This function will receive multiple files.
 */
char multiReceive(struct fl_req *file_data)
{
    char	first = 1;
    extern char *WRITE_ANY;

    sprintf(msgBuf.mbtext, "File Request: %s from %s on %s stored in %s: ",
	file_data->roomfile, file_data->room, netBuf.netName,
	prtNetArea(&file_data->flArea));
    do {
	if (ITL_Receive(NULL, FALSE, TRUE, putFLChar, fclose) != ITL_SUCCESS ||
		!gotCarrier()) return ITL_BAD_TRANS;
	if (RecBuf[0] == 0) {	/* Last file name       */
	    strcat(msgBuf.mbtext, ".");
	    netResult(msgBuf.mbtext);
	    return ITL_SUCCESS;
	}
	if (!first)
	    strcat(msgBuf.mbtext, ", ");
	else
	    first = FALSE;
	strcat(msgBuf.mbtext, RecBuf);
	if (ITL_Receive(RecBuf, FALSE, TRUE, putFLChar, fclose) != ITL_SUCCESS
		|| !gotCarrier()) return ITL_BAD_TRANS;
    } while (1);
    return ITL_SUCCESS;
}

/*
 * sendNetCommand()
 *
 * This sends a command to the receiver.
 */
char sendNetCommand(struct cmd_data *cmds, char *error)
{
    char errMsg[100];
    int  count;

    if (!ITL_Send(STARTUP)) {
	sprintf(errMsg, "Link failure for %s (system: %%s).", error);
	if (cmds->command != HANGUP) no_good(errMsg, TRUE);
	killConnection("snc");
	return FALSE;
    }
    sendITLchar(cmds->command);
    for (count = 0; count < 4; count++) {
	if (cmds->fields[count][0]) {
	    NetPrintf(sendITLchar, "%s", cmds->fields[count]);
	}
    }
    sendITLchar(0);
    ITL_Send(FINISH);
    if (cmds->command == HANGUP && !inReceive) return TRUE;
    ITL_Receive(NULL, FALSE, TRUE, putFLChar, fclose);
    if (RecBuf[0] == BAD || !gotCarrier()) return FALSE;
    return TRUE;
}

/*
 * sendHangUp()
 *
 * This sends the hangup command to receiver.
 */
void sendHangUp()
{
    struct cmd_data cmds;

    if (!gotCarrier()) {
	modStat = haveCarrier = FALSE;
	return ;
    }
    zero_struct(cmds);
    cmds.command = HANGUP;
    sendNetCommand(&cmds, "HANGUP");
}

/*
 * no_good()
 *
 * This handles error messages when something really bad happens.
 */
void no_good(char *str, char hup)
{
    sprintf(msgBuf.mbtext, str, netBuf.netName);
    if (hup) {
	killConnection("no good");
    }
    netResult(msgBuf.mbtext);
}

/*
 * send_direct_mail()
 *
 * This sends mail normal (non-route mail).
 */
int send_direct_mail(int which, char *name)
{
    FILE     *ptrs;
    label    fntemp;
    SYS_FILE fn;
    int      messCount = 0;
    struct   netMLstruct buf;
    extern char *READ_ANY;

    sprintf(fntemp, "%d.ml", which);
    makeSysName(fn, fntemp, &cfg.netArea);
    if ((ptrs = fopen(fn, READ_ANY)) == NULL) {
	if (netTab[which].ntflags.normal_mail) {
	    sprintf(msgBuf.mbtext, "Send Mail: Mail file for %s missing.",
							netBuf.netName);
	    netResult(msgBuf.mbtext);
	}
	return 0;
    }

    while (getMLNet(ptrs, buf) && TrError == TRAN_SUCCESS) {
	if (findMessage(buf.ML_loc, buf.ML_id, TRUE)) {
	    prNetStyle(FALSE, getMsgChar, sendITLchar, TRUE, name);
	    messCount++;
	}
    }

    fclose(ptrs);
    if (TrError == TRAN_SUCCESS) {
	unlink(fn);
	return messCount;
    }

    killConnection("sdm");
    splitF(netLog, "\nFailed transferring mail!\n");
    return 0;
}

/*
 * SendHostFile()
 *
 * This function will send a file to the other system.
 */
void SendHostFile(char *fn)
{
    int  success;
    FILE *fd;
    extern int errno;
    extern char *READ_ANY;

    success = ((fd = safeopen(fn, READ_ANY)) != NULL);
    if (ITL_Send(STARTUP)) {
	if (!success) NetPrintf(sendITLchar,
			"System error failure, this is a bogus file.");
	else {
	    SendThatDamnFile(fd, sendITLchar);
	}
	ITL_Send(FINISH);
    }
    if (!success) {
	sprintf(msgBuf.mbtext,
		"SendHostFile: Couldn't open %s for %s, errno %d.",
						fn, netBuf.netName, errno);
	netResult(msgBuf.mbtext);
    }
}
