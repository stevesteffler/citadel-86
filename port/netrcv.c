/*
 *				netrcv.c
 *
 *      Networking functions for reception.
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
 *				Contents
 *
 *	called()		Handle being called.
 *	rcvStuff()		Manage receiving stuff.
 *	netPwd()		Check password.
 *	doResults()		Post-process results.
 *	getId()			Get nodeId and nodeName from caller.
 *	getNextCommand()	Get next command.
 *	grabCommand()		Extract network cmds from buffer.
 *	reply()			Sends a reply to caller.
 *	reqReversal()		Handle role reversal.
 *	reqCheckMail()		Check incoming mail.
 *	targetCheck()		Check for existence of recipients.
 *	CheckRecipient()	work fn for above.
 *	doNetRooms()		Manage integrating incoming messages.
 *	IntegrateRoom()		work fn for above.
 *	ReadNetRoomFile()	work fn for above.
 *	getMail()		Handle incoming mail.
 *	reqSendFile()		Receive a sent file.
 *	netFileReq()		Senda a requested file.
 *	netRRReq()		Handle room sharing.
 *	recNetMessages()	Receive net messages.
 *	UpdateRecoveryFile()	Updates the network recovery file.
 * 	RoomRoutable()		Is this room routable?
 *	IsRoomRoutable()	Is this room routable?
 *	netMultiSend()		Send multiple files.
 *	RecoverNetwork()	Recover from network disaster.
 */

char netDebug = FALSE;
char		processMail;
char		PosId;
char		*AssignAddress = NULL;

#define RECOVERABLE	1

extern char		*SR_Sent;
extern FILE		*netLog, *netMisc;
extern AN_UNSIGNED      RecBuf[];
extern int		counter;
extern int		callSlot;
extern char		checkNegMail;
extern char		inReceive;
extern label		normed, callerName, callerId;
extern char		RouteMailForHere;
extern char		*LOC_NET, *NON_LOC_NET;
extern long		TransferTotal;

char		normId(), getNetMessage();

char *netRoomTemplate = "room%d.$$$";
char *SharingRefusal[] = {
	"'%s' does not exist",
	"'%s' is not a networking room",
	"'%s' is not networking with you",
	"No can do for '%s'",
};

extern char	 RecMassTransfer;
extern CONFIG    cfg;		/* Lots an lots of variables    */
extern logBuffer logBuf;	/* Person buffer		*/
extern logBuffer logTmp;	/* Person buffer		*/
extern aRoom     roomBuf;	/* Room buffer		*/
extern rTable    *roomTab;
extern MessageBuffer   msgBuf;
extern NetBuffer netBuf;
extern NetTable  *netTab;
extern int       thisNet;
extern char      onConsole;
extern char      loggedIn;	/* Is we logged in?		*/
extern char      outFlag;	/* Output flag		*/
extern char      haveCarrier;	/* Do we still got carrier?     */
extern char      modStat;	/* Needed so we don't die       */
extern char      WCError;
extern int       thisRoom;
extern int       thisLog;
extern char	 *R_SH_MARK, *NON_LOC_NET, *LOC_NET;

/*
 * called()
 *
 * We've been called, so let's handle it.
 */
void system_called()
{
    ITL_InitCall();		/* Initialize the ITL layer */

    inReceive = TRUE;
    RecMassTransfer = FALSE;
    splitF(netLog, "Carrier %s\n", Current_Time());
    processMail = checkNegMail = FALSE;

    if (!called_stabilize()) return ;

    splitF(netLog, "Stabilized\n");

    getId();
    if (!haveCarrier) return;

    rcvStuff(FALSE);

    splitF(netLog, "Finished with %s @%s\n",
			callerName, Current_Time());
    pause(20);
    killConnection("called");
    doResults();
    splitF(netLog, "\n");
}

/*
 * rcvStuff()
 *
 * This function manages receiving stuff.
 */
void rcvStuff(char reversed)
{
    label	    tempNm;
    struct cmd_data cmds;

    PosId = (callSlot == ERROR) ? FALSE : (netBuf.OurPwd[0] == 0 || (reversed && netBuf.nbflags.Stadel));
    RouteMailForHere = FALSE;

    do {
	getNextCommand(&cmds);
	switch (cmds.command) {
	    case HANGUP:					break;
	    case NORMAL_MAIL:    getMail();			break;
	    case A_FILE_REQ:
	    case R_FILE_REQ:     netFileReq(&cmds);		break;
	    case NET_ROOM:       netRRReq(&cmds, FALSE);	break;
	    case ROLE_REVERSAL:  reqReversal(&cmds, reversed);	break;
	    case CHECK_MAIL:     reqCheckMail();		break;
	    case SF_PROTO:
	    case SEND_FILE:      reqSendFile(&cmds);		break;
	    case NET_ROUTE_ROOM: netRRReq(&cmds, TRUE);		break;
	    case SYS_NET_PWD:    netPwd(&cmds);			break;
	    case ITL_PROTOCOL:   ITL_rec_optimize(&cmds);       break;
	    case ITL_COMPACT:    ITL_RecCompact(&cmds);		break;
	    case ROUTE_MAIL:     netRouteMail(&cmds);		break;
	    case FAST_MSGS:	 netFastTran(&cmds);		break;
	    default:		 sprintf(tempNm, "'%d' unknown.", cmds.command);
				 reply(BAD, tempNm);		break;
	}
    } while (gotCarrier() && cmds.command != HANGUP);
}

/*
 * netPwd()
 *
 * Check out the password sent to us, set flags appropriately.
 */
void netPwd(struct cmd_data *cmds)
{
    if (callSlot != ERROR) {
	PosId = !strCmpU(cmds->fields[0], netBuf.OurPwd);
	if (!PosId) {
	    splitF(netLog, "Bad pwd: -%s-\n", cmds->fields[0]);
	    sprintf(msgBuf.mbtext, "%s sent bad password -%s-.",
			callerName, cmds->fields[0]);

	    netResult(msgBuf.mbtext);
	}
    }
    reply(GOOD, "");
}

/*
 * doResults()
 *
 * This function processes the results of receiving thingies and such.
 */
void doResults()
{
	SYS_FILE tempNm;
	extern SListBase DomainMap;
	void HandleExistingDomain();
	int i;
	label temp;
	extern int RMcount;
	extern char *FastTemp;

	DisableModem(TRUE);
	InitVortexing();		/* handles all mail for here */
	if (processMail) {
		if (cfg.BoolFlags.SiegeNet && callSlot == ERROR) {
			makeSysName(tempNm, "tempmail.$$$", &cfg.netArea);
			unlink(tempNm);
		}
		else {
			if (AddNetMsgs("tempmail.$$$", inMail, 2, MAILROOM,
							TRUE) == ERROR)
				no_good("No mail file for %s?", TRUE);
		}
	}
	if (RouteMailForHere) {
		for (i = 0; ; i++) {
			sprintf(temp, "rmail.%d", i);
			if (AddNetMsgs(temp, inRouteMail, 2, MAILROOM, TRUE)
								== ERROR)
				break;
		}
		RMcount = 0;
	}
	FinVortexing();		/* finish handling mail */
	if (callSlot == ERROR) {    /* If didn't know this node, don't      */
		EnableModem(TRUE);
		return ;	/* bother with anything else	*/
	}
	if (checkNegMail)	readNegMail(TRUE);
	ReadFastFiles(FastTemp);  /* messages transferred in one big file */
	doNetRooms();
	AdjustRoute();
	netBuf.nbLastConnect = CurAbsolute();
	putNet(thisNet, &netBuf);
	UpdVirtStuff(TRUE);	/* Just in case. */
	RunList(&DomainMap, HandleExistingDomain);
	RationalizeDomains();	/* again just in case ... */
	UpdateSharedRooms();
	EnableModem(TRUE);
}

/*
 * getId()
 *
 * This gets nodeId and nodeName from caller.
 */
void getId()
{
	extern SListBase	SystemsCalled;
	extern SListBase	Shutup;
	int NotSentYet(SharedRoomData *room, int system, int roomslot, void *d);
	char                    *secRunner;
#ifdef NEED_THIS_DATA
	SYS_FILE                fn;
#endif
	extern UNS_32 BaudRate;

	if (!haveCarrier) return;
	ITL_Receive(NULL, FALSE, TRUE, putFLChar, fclose);
	if (!gotCarrier()) {
		return ;
	}
	strncpy(callerId, RecBuf, NAMESIZE - 1);
	secRunner = RecBuf;
	while (*secRunner != 0) secRunner++;
	secRunner++;
	strncpy(callerName, secRunner, NAMESIZE - 1);
	normId(callerId, normed);
	if (strLen(callerName) == 0 || strLen(callerId) == 0) {
		splitF(netLog, "getId invalid data, dropping connection.\n\n");
		killConnection("getid");
#ifdef NEED_THIS_DATA
		makeSysName(fn, "getid.sys", &cfg.netArea);
		if ((upfd = fopen(fn, APPEND_ANY)) != NULL) {
			fwrite(RecBuf, SECTSIZE, 1, upfd);
			fclose(upfd);
		}
#endif
	}
	if ((callSlot = searchNet(normed, &netBuf)) == ERROR) {
#ifdef NEEDED
		if (SearchList(&Shutup, callerName) == NULL) {
#endif
			sprintf(msgBuf.mbtext, "New caller: %s (%s)",
				callerName, callerId);
			netResult(msgBuf.mbtext);
#ifdef NEEDED
		}
#endif
	}
	else {
		if (SearchList(&SystemsCalled, &callSlot) == NULL)
			NewCalledRecord(callSlot);
	}

	EachSharedRoom(thisNet, NotSentYet, NotSentYet, NULL);
	splitF(netLog, "%s (%s) @ %ld\n", callerName, callerId, BaudRate);
}

/*
 * NotSentYet()
 *
 * Turns off the sent flag.
 */
static int NotSentYet(SharedRoomData *room, int system, int roomslot, void *d)
{
    room->room->sr_flags &= ~SR_SENT;
    return TRUE;
}

/*
 * getNextCommand()
 *
 * This gets next command (facility request) from the caller.
 */
void getNextCommand(struct cmd_data *cmds)
{
    zero_struct(*cmds);

    ITL_Receive(NULL, FALSE, TRUE, putFLChar, fclose);
    if (!gotCarrier()) {
	return ;
    }
    grabCommand(cmds, RecBuf);
}

/*
 * grabCommand()
 *
 * This pulls network cmds out of the specified buffer.
 */
void grabCommand(struct cmd_data *cmds, char *sect)
{
    int fcount = 0;

    cmds->command = sect[0];
    if (cfg.BoolFlags.debug)
	splitF(netLog, "Command byte: %d\n", cmds->command);
    sect++;
    while (sect[0] > 0 && fcount < 4) {
	strncpy(cmds->fields[fcount], sect, NAMESIZE - 1);
	cmds->fields[fcount][NAMESIZE - 1] = 0;
	fcount++;
	while (*sect != 0) sect++;
	sect++;
    }
}

/*
 * reply()
 *
 * This sends a full reply to the caller's request.
 */
void reply(char state, char *reason)
{
    if (!ITL_Send(STARTUP)) {
	no_good("Couldn't send reply to %s!", TRUE);
	return;
    }
    sendITLchar(state);
    if (state == BAD) {
	NetPrintf(sendITLchar, "%s", reason);
	if (cfg.BoolFlags.debug)
	    splitF(netLog, "Replying BAD: %s\n", reason);
    }
    sendITLchar(0);
    ITL_Send(FINISH);
}

/*
 * reqReversal()
 *
 * This handles the role reversal command.
 */
static void reqReversal(struct cmd_data *cmds, char reversed)
{
	if (cfg.BoolFlags.debug) splitF(netLog, "Role reversal\n");

	if (reversed) {
		reply(BAD, "Synch error on Reversal!");
		return ;
	}

	reply(GOOD, "");
	if (callSlot == ERROR)      /* Forces a "null" role reversal	*/
		zero_struct(netBuf.nbflags);

	if (strcmp(cmds->fields[0], "interrupted") == 0) {
		sendStuff(TRUE, PosId, FALSE);
	}
	else sendStuff(TRUE, PosId, TRUE);
}

/*
 * reqCheckMail()
 *
 * This checks incoming mail and does negative acks where appropriate.
 */
static void reqCheckMail()
{
    splitF(netLog, "checking Mail\n");

    if (!processMail) {
	reply(BAD, "No mail to check!");
	return ;
    }

    reply(GOOD, "");

    if (ITL_Send(STARTUP)) {
	AddNetMsgs("tempmail.$$$", targetCheck, FALSE, MAILROOM, TRUE);
	sendITLchar(NO_ERROR);
	ITL_Send(FINISH);
    }
}

/*
 * targetCheck()
 *
 * This checks for existence of recipients.
 */
void targetCheck()
{
    if (HasOverrides(&msgBuf)) {
	RunList(&msgBuf.mbOverride, CheckRecipient);
    }
    else {
	CheckRecipient(msgBuf.mbto);
    }
}

/*
 * CheckRecipient()
 *
 * This will check to see if the recipient exists.
 */
void CheckRecipient(char *d)
{
	char     sigChar = NO_ERROR;
	int author, target;

	if (!d[0]) {
		sigChar = BAD_FORM;
	}
	else {
		if ((target = PersonExists(d)) != ERROR) {
			if ((author = PersonExists(msgBuf.mbauth)) != ERROR) {
				if (!AcceptableMail(author, target)) {
					sigChar = NO_RECIPIENT;
				}
			}
			if (sigChar == NO_ERROR)
				return ;
		}
		else
			sigChar = NO_RECIPIENT;
	}
	sendITLchar(sigChar);
	NetPrintf(sendITLchar, "%s", msgBuf.mbauth);
	NetPrintf(sendITLchar, "%s", d);
	NetPrintf(sendITLchar, "%s @ %s", msgBuf.mbdate, msgBuf.mbtime);
}

/*
 * doNetRooms()
 *
 * This function integrates temporary files containing incoming messages into
 * the data base.
 */
static void doNetRooms()
{
    SYS_FILE fileNm;
    int IntegrateRoom(SharedRoomData *room, int system, int roomslot, void *d);

    EachSharedRoom(thisNet, IntegrateRoom, NULL, NULL);
    makeSysName(fileNm, RECOVERY_FILE, &cfg.netArea);
    unlink(fileNm);
}

/*
 * IntegrateRoom()
 *
 * This function helps integrate incoming messages into a room.
 */
int IntegrateRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
    if (room->room->sr_flags & SR_RECEIVED) {
	    ReadNetRoomFile(room->room, NULL);
	    room->room->sr_flags &= ~SR_RECEIVED;
    }
    if (room->room->sr_flags & SR_SENT) {
	/*
	 * bug 119.668 -> changed from just rtlastMess to max of that and
	 * lastNetBB.
	 * bug 119.673 -> added rtlastNetAll to the max equation.  Fixes
	 * problem with endless retries due to deleted message.
	 */
	room->room->lastMess = max(roomTab[roomslot].rtlastNetAll, max(roomTab[roomslot].rtlastMessage, roomTab[roomslot].rtlastNetBB));
	room->room->sr_flags &= ~SR_SENT;
	room->srd_flags |= SRD_DIRTY;
    }
    return TRUE;
}

/*
 * ReadNetRoomFile()
 *
 * This function reads in a file of messages received on net.
 * NB: the passed parameter is the index into the netBuf.netRooms
 * pseudo-array, not the number of the room itself.  See the code.
 */
void ReadNetRoomFile(SharedRoom *room, char *fn)
{
    label temp2;

    if (fn == NULL) sprintf(temp2, netRoomTemplate, netRoomSlot(room));
    getRoom(netRoomSlot(room));

    if (GetMode(room->mode) != PEON)
	AssignAddress = NON_LOC_NET;
    else
	AssignAddress = LOC_NET;

    InitVortexing();
    AddNetMsgs((fn == NULL) ? temp2 : fn, inMail, TRUE, netRoomSlot(room),
								(fn == NULL));
    FinVortexing();

    AssignAddress = NULL;
}

/*
 * getMail()
 *
 * This function Grabs mail from caller.
 */
static void getMail()
{
	SYS_FILE tempNm;

	splitF(netLog, "Receiving Mail =>");
	makeSysName(tempNm, "tempmail.$$$", &cfg.netArea);
	if (ITL_StartRecMsgs(tempNm, TRUE, TRUE, NULL) == ITL_SUCCESS) {
		processMail = TRUE;
		splitF(netLog, " %ld bytes\n", TransferTotal);
	}
	else	splitF(netLog, " Failed\n");
}

/*
 * reqSendFile()
 *
 * This function handles receiving a sent file.  Note that it handles file
 * redirection.
 */
void reqSendFile(struct cmd_data *cmds)
{
	long  proposed;
	int count, proto;
	char work[100], work1[100], *Dir;
	PROTOCOL *External;
	extern char *READ_ANY, *WRITE_ANY;
	static char *Reject = "File Reception: %s from %s rejected, %s.";

	/* don't accept files from rogues */
	if (!PosId) {
		reply(BAD, "No room for file.");
		return;
	}

	proto = atoi(cmds->fields[3]);
	if (cmds->command == SF_PROTO) {
		if ((proto = ReceiveProtocolValidate(proto,
					&External)) == BAD_PROTOCOL)
			return;
	}

    /* handle incoming file redirection */
	if ((Dir = RedirectFile(cmds->fields[0], netBuf.netName)) != NULL ||
			(Dir = RedirectFile(cmds->fields[0], netBuf.nbShort))
						!= NULL) {
		RedirectName(work1, Dir, "mm12");	/* temp file name */
		unlink(work1);	/* kill any prior backups */
		RedirectName(work, Dir, cmds->fields[0]);
		if (access(work, 0) == 0)
			rename(work, work1);
	}
	else if (netSetNewArea(&cfg.receptArea)) {
		proposed = atol(cmds->fields[2]);
		if (sysRoomLeft() < proposed ||
				proposed > ((long) cfg.maxFileSize) * 1024l) {
			reply(BAD, "No room for file.");
			sprintf(msgBuf.mbtext, Reject,
					cmds->fields[0], callerName,
				proposed > ((long) cfg.maxFileSize) * 1024l ?
				"the file was larger than #MAX_NET_FILE" :
				"there was not enough room left in reception directory");
			netResult(msgBuf.mbtext);
			homeSpace();
			return;
		}
		count = 0;
		strcpy(work, cmds->fields[0]);
		while (access(work, 0) != -1) {
			sprintf(work, "a.%d", count++);
		}
	}
	else {
		reply(BAD, "System error");
		sprintf(msgBuf.mbtext, "File Reception error, could not move to '%s', errno %d\n", prtNetArea(&cfg.receptArea), errno);
		splitF(netLog, "%s\n", msgBuf.mbtext);
		netResult(msgBuf.mbtext);
		return ;
	}
	reply(GOOD, NULL);
	splitF(netLog, "File Reception: %s\n", cmds->fields[0]);
	if (cmds->command == SF_PROTO && proto == ZM_PROTOCOL) {
		ExternalTransfer(External, work);
		while (MIReady()) Citinp();
	}
	else {
		/*
		 * depends on ReceiveProtocolValidate() not allowing anything
		 * besides "default" to get through.  Will need work to allow
		 * complete protocol freedom.
		 */
		ITL_Receive(work, FALSE, TRUE, putFLChar, fclose);
	}

	if (access(work, 0) != 0) {
		sprintf(msgBuf.mbtext, "Reception of %s from %s failed.",
					cmds->fields[0], netBuf.netName);
		netResult(msgBuf.mbtext);
		splitF(netLog, "%s\n", msgBuf.mbtext);
	}
	else {
		if (haveCarrier && Dir == NULL) {
			msgBuf.mbtext[0] = 0;
			if (strcmp(work, cmds->fields[0]) != SAMESTRING)
				sprintf(msgBuf.mbtext, "(Real name %s) ",
							cmds->fields[0]);
			sprintf(msgBuf.mbtext+strlen(msgBuf.mbtext),
			"Received from %s on %s.", netBuf.netName, formDate());
				/* for updating filedir.txt */
			netSetNewArea(&cfg.receptArea);
			updFiletag(work, msgBuf.mbtext);
		}
		if (haveCarrier) {
			strcpy(msgBuf.mbtext, "File Reception: ");
			if (strcmp(work, cmds->fields[0]) == SAMESTRING ||
					Dir != NULL)
		    		sprintf(lbyte(msgBuf.mbtext),
					"%s received from %s.",
					cmds->fields[0], callerName);
			else
				sprintf(lbyte(msgBuf.mbtext),
					"%s (saved as %s) received from %s.",
					cmds->fields[0], work, callerName);
			netResult(msgBuf.mbtext);
			/* kill temporary bkp of redirected file */
			if (Dir != NULL)
				unlink(work1);
		}
		else if (Dir != NULL)	/* failed transfer of redirected file */
			rename(work1, work);
	}
	homeSpace();
}

/*
 * netFileReq()
 *
 * This will handle requests for file transfers.
 */
void netFileReq(struct cmd_data *cmds)
{
    int  roomSlot;
    extern char *READ_ANY;

    splitF(netLog, "File request: %s in %s\n", cmds->fields[1],
					cmds->fields[0]);

    /* allow disabling this feature on a system by system basis */
    if (PosId && netBuf.nbflags.NoDL) {
	reply(BAD, "Downloading disabled.");
	return;
    }

    if ((roomSlot = roomExists(cmds->fields[0])) == ERROR   ||
			!roomTab[roomSlot].rtflags.ISDIR  ||
			roomTab[roomSlot].rtflags.NO_NET_DOWNLOAD ||
			!roomTab[roomSlot].rtflags.DOWNLOAD) {
	sprintf(msgBuf.mbtext, "Room %s does not exist.", cmds->fields[0]);
	reply(BAD, msgBuf.mbtext);
	return;
    }

    getRoom(roomSlot);
    if (!SetSpace(FindDirName(thisRoom))) {
	reply(BAD, "Directory error");
	return;
    }

    sprintf(msgBuf.mbtext, "File Request: Sent to %s from %s - ",
					callerName, roomBuf.rbname);
    if (cmds->command == A_FILE_REQ) {
	reply(GOOD, "");
splitF(netLog, "A_FileReq reply(GOOD)\n");
	wildCard(netMultiSend, cmds->fields[1], "", WC_NO_COMMENTS);
	if (ITL_Send(STARTUP)) {
	    NetPrintf(sendITLchar, "");
	    ITL_Send(FINISH);
	}
    }
    else {
	if (access(cmds->fields[1], 4) == -1) {
	    sprintf(msgBuf.mbtext, "There is no '%s' in %s.", cmds->fields[1],
							cmds->fields[0]);
	    reply(BAD, msgBuf.mbtext);
	    homeSpace();
	    return;
	}

	reply(GOOD, "");
splitF(netLog, "R_FileReq reply(GOOD)\n");

	SendHostFile(cmds->fields[1]);
	sprintf(lbyte(msgBuf.mbtext), "%s", cmds->fields[1]);
    }
    homeSpace();
    netResult(msgBuf.mbtext);
}

/*
 * netRRReq()
 *
 * This function handles room sharing.  If SendBack is TRUE then this is a
 * room routing (LD) request and requires we send the room's current contents
 * back.
 */
void netRRReq(struct cmd_data *cmds, char SendBack)
{
    extern VirtualRoom *VRoomTab;
    RoomSearch arg;
    char reason[50];

    strcpy(arg.Room, cmds->fields[0]);
    if (!RoomRoutable(&arg)) {
	sprintf(reason, SharingRefusal[arg.reason], cmds->fields[0]);
	splitF(netLog, "Refusing to share %s (%s)\n", cmds->fields[0], reason);
	reply(BAD, reason);
	return;
    }

    if (!arg.virtual) {
	getRoom(arg.roomslot);

	recNetMessages(arg.room->room, arg.Room, arg.roomslot, TRUE);

	if (SendBack)
	    findAndSend(ERROR, arg.room, RoomSend, roomBuf.rbname, RoomReceive);
    }
    else {
	RecVirtualRoom(arg.room, TRUE);
	if (SendBack) {
	    findAndSend(ERROR, arg.room, SendVirtual,
				VRoomTab[arg.roomslot].vrName, RecVirtualRoom);
	}
    }
}

/*
 * recNetMessages()
 *
 * This function receives net messages.  This is not the same as processing
 * them.
 */
void recNetMessages(SharedRoom *room, char *name, int slot, char ReplyFirst)
{
    SYS_FILE fileNm, temp;
    char reason[60];

    splitF(netLog, "Receiving %s =>", name);
    sprintf(temp, netRoomTemplate, slot);
    makeSysName(fileNm, temp, &cfg.netArea);
    switch (ITL_StartRecMsgs(fileNm, ReplyFirst, TRUE, NULL)) {
	case ITL_SUCCESS:
	    splitF(netLog, " %ld bytes\n", TransferTotal);
	    room->sr_flags |= SR_RECEIVED;
	    UpdateRecoveryFile(name);
	    break;
	case ITL_NO_OPEN:
	    sprintf(reason, "Internal error for %s", name);
	    reply(BAD, reason);
	    splitF(netLog, " Failed\n");
	    break;
	case ITL_BAD_TRANS:
	    splitF(netLog, " Failed\n");
	    break;
    }
}

/*
 * UpdateRecoveryFile()
 *
 * This function updates the network recovery file.
 */
void UpdateRecoveryFile(char *val)
{
    SYS_FILE fileNm;

    makeSysName(fileNm, RECOVERY_FILE, &cfg.netArea);
    if (access(fileNm, 0) != 0)
	CallMsg(fileNm, callerId);

    CallMsg(fileNm, val);
}

/*
 * RoomRoutable()
 *
 * Is this room routable?
 */
char RoomRoutable(RoomSearch *data)
{
    extern char inNet;

    if (inNet != NON_NET) {
	if (!PosId) {
	    data->reason = NO_PWD;
	    return FALSE;
	}

	if (callSlot == ERROR) {
	    data->reason = NOT_SHARING;
	    return FALSE;
	}
    }

    data->reason = NO_ROOM;
    data->virtual = FALSE;
    EachSharedRoom(thisNet, IsRoomRoutable, VirtRoomRoutable, data);
    return (data->reason == FOUND);
}

/*
 * IsRoomRoutable()
 *
 * This function checks to see if the given shared room matches with the
 * argument presented in the void pointer parameter.
 */
int IsRoomRoutable(SharedRoomData *room, int system, int roomslot, void *d)
{
    RoomSearch *arg;

    arg = d;
    if (strCmpU(roomTab[roomslot].rtname, arg->Room) == SAMESTRING) {
	if (roomTab[roomslot].rtflags.SHARED == 0)
	    arg->reason = NOT_SHARING;
	else
	    arg->reason = FOUND;
	arg->roomslot = roomslot;
	arg->room     = room;
	return ERROR;		/* stop searching */
    }
    return TRUE;
}

/*
 * netMultiSend()
 *
 * This function will send requested files via the net.
 */
void netMultiSend(DirEntry *fn)
{
    long Sectors;

    if (!gotCarrier()) return ;

    strcat(msgBuf.mbtext, fn->unambig);
    strcat(msgBuf.mbtext, " ");
    Sectors     = ((fn->FileSize + 127) / SECTSIZE);
    if (ITL_Send(STARTUP)) {
splitF(netLog, "nMS sending name & size\n");
	NetPrintf(sendITLchar, "%s", fn->unambig);
	NetPrintf(sendITLchar, "%ld", Sectors);
	ITL_Send(FINISH);
splitF(netLog, "nMS finished sending name & size\n");
    }
splitF(netLog, "about to do SHF\n");
    SendHostFile(fn->unambig);
}

/*
 * RecoverNetwork()
 *
 * This function is called during system startup.  If a disaster hit during
 * a network session, this will try to recover messages already transferred
 * from files left in the network directory.
 */
void RecoverNetwork()
{
    SYS_FILE fileNm;
    char line[50];
    label temp;
    int rover;
    FILE *fd;
    RoomSearch arg;
    extern char inNet;

    makeSysName(fileNm, RECOVERY_FILE, &cfg.netArea);
    if ((fd = fopen(fileNm, READ_TEXT)) == NULL)
	return;		/* normal */

    inNet = NORMAL_NET;
    SpecialMessage("Network Cleanup");
    if (GetAString(line, sizeof line, fd) != NULL) {
	if (searchNet(line, &netBuf) != ERROR) {
	    PosId = TRUE;
	    while (GetAString(line, sizeof line, fd) != NULL) {
		if (strncmp(line, FAST_TRANS_FILE, strLen(FAST_TRANS_FILE))
								== SAMESTRING)
		    RecoverMassTransfer(line);
		else {
		    strcpy(arg.Room, line);
		    if (RoomRoutable(&arg) && !arg.virtual) {
			printf("%s\n", line);
			ReadNetRoomFile(arg.room->room, NULL);
		    }
		}
	    }
	    readNegMail(FALSE);
	    AddNetMsgs("tempmail.$$$", inMail, 2, MAILROOM, TRUE);
	    for (rover = 0; ; rover++) {
		sprintf(temp, "rmail.%d", rover);
		if (AddNetMsgs(temp, inRouteMail, 2, MAILROOM, TRUE) == ERROR)
		    break;
	    }
	}
    }
    fclose(fd);
    unlink(fileNm);
    inNet = NON_NET;
}

