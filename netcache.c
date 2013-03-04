/*
 *				netcache.c
 *
 * Networking functions for handling network cache (fast transfers).
 */

/*
 *				history
 *
 * 91Sep20 HAW	Created.
 */
#define NET_INTERNALS

#include "ctdl.h"
#include "compress.h"

#include <sys\stat.h>

#define MAP_FILE	"map.$$$"
#define FAST_TEMPLATE	"fast$$$.%s"
#define NETMSGS		"netmsgs.%s"

#define I_SEP		'\t'
/*
 *				contents
 *
 */
char RecMassTransfer;
char MassTransferSent;
char *FastTemp;
static int MTCompVal;

extern CONFIG    cfg;		/* Lots an lots of variables    */
extern char      *SR_Sent;
extern MessageBuffer	 msgBuf;
extern char      inReceive;
extern NetBuffer netTemp;
extern char      inNet;
extern NetBuffer netBuf;
extern FILE	 *netLog;
extern rTable    *roomTab;
extern FILE	 *upfd;
extern int	 callSlot;
extern NetTable  *netTab;
extern int       thisNet;
extern char	 *READ_ANY, *APPEND_ANY;

/*
 * FileMap
 *
 * This structure is used to map between a filename and the room it's
 * carrying.
 */
typedef struct {
    char *FileName;
    char *RoomName;
} FileMap;

/*
 * Mappings
 *
 * This is a list of mappings for our current work.
 */
void FreeMapping();
SListBase Mappings = { NULL, NULL, NULL, FreeMapping, NULL };

int RoomOutgoing(SharedRoomData *room, int system, int roomslot, char *d);
int UnCacheRoom(SharedRoomData *room, int system, int roomslot, void *d);
/*
 * SendFastTransfer()
 *
 * We want to use Facility 21, the mass transfer of messages.
 */
char SendFastTransfer()
{
	PROTOCOL *External;
	long   size;
	extern long netBytes;
	extern AN_UNSIGNED RecBuf[SECTSIZE + 5];
	struct cmd_data cmds;
	char BaseArcName[15], Outgoing;
	int protocol;
	char ArcFileName[100];
	char CacheDir[70];
char buf[100];

	if (!netBuf.nbflags.MassTransfer) {
		return FALSE;
	}

	if (!netBuf.nbflags.local && !(netBuf.nbflags.spine || inReceive)) {
		splitF(netLog, "Mass Transfer flag ignored\n");
		return FALSE;
	}

	if (!CompAvailable(netBuf.nbCompress)) {
		return FALSE;
	}

	/* do we have rooms to send??? */
	Outgoing = FALSE;
	EachSharedRoom(thisNet, RoomOutgoing, VirtualRoomOutgoing,
							(void *) &Outgoing);
	if (!Outgoing) {
		MassTransferSent = TRUE;
		EachSharedRoom(thisNet, UnCacheRoom, UnCacheVirtualRoom, NULL);
		return FALSE;
	}

	zero_struct(cmds);
	sprintf(cmds.fields[0], "%d", netBuf.nbCompress);
	sprintf(cmds.fields[1], "%d", ZM_PROTOCOL);
	strCpy(cmds.fields[2], "0");
	strCpy(cmds.fields[3], "-1");
	cmds.command = FAST_MSGS;
	protocol = ZM_PROTOCOL;
	if ((External = FindProtocolByName("Zmodem", FALSE)) == NULL ||
					!sendNetCommand(&cmds, "mt")) {
		sprintf(cmds.fields[1], "%d", DEFAULT_PROTOCOL);
		protocol = DEFAULT_PROTOCOL;
		if (!sendNetCommand(&cmds, "mt")) {
			splitF(netLog, "Fast Transfer refused\n");
			return FALSE;
		}
	}

	if (SendMapFile()) {
		if (!MapFileAccepted()) {
			return FALSE;
		}
		CacheSystem(thisNet, FALSE);
		outMod(ACK);
		sprintf(BaseArcName, NETMSGS, CompExtension(netBuf.nbCompress));
		NetCacheName(ArcFileName, thisNet, BaseArcName);
		splitF(netLog, "Mass Transfer\n");
		if (protocol == DEFAULT_PROTOCOL)
			SendHostFile(ArcFileName);
		else {
			ExternalTransfer(External, ArcFileName);
			while (MIReady()) Citinp();
			pause(100);	/* keep the other system from eating
								our NAK */
		}

		if (gotCarrier()) {
			ITL_Receive(NULL, FALSE, TRUE, putFLChar, fclose);
			if (RecBuf[0] == BAD || !gotCarrier())
				return FALSE;
			MassTransferSent = TRUE;
			EachSharedRoom(thisNet, UnCacheRoom, UnCacheVirtualRoom,
									NULL);
			MakeNetCacheName(CacheDir, thisNet);
			if (ChangeToCacheDir(CacheDir) == 0) {
				netBytes = 0l;
				wildCard(getSize, BaseArcName, "",
								WC_NO_COMMENTS);
				size = netBytes;
				netBytes = 0l;
				wildCard(getSize, CACHED_FILES, "",
								WC_NO_COMMENTS);
				wildCard(DelFile, ALL_FILES, "",
								WC_NO_COMMENTS);
				splitF(netLog, "MT: %ld => %ld\n", netBytes, size);
			}
else splitF(netLog, "CTCD(%s) for call erasure failed %d, pwd is -%s-\n", CacheDir, errno, getcwd(buf, sizeof buf));
			homeSpace();
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * RoomOutgoing()
 *
 * This is responsible for deciding if the given shared room has outgoing
 * material in a room eligible for mass transfers.
 */
static int RoomOutgoing(SharedRoomData *room, int system, int roomslot, char *d)
{
    if (strLen(roomTab[roomslot].rtname) && HasOutgoing(room->room)) {
	*d = TRUE;
	return ERROR;
    }
    return TRUE;
}

/*
 * SendMapFile()
 *
 * This sends a file containing the names of rooms to share and the filenames
 * in the upcoming archive file to map the rooms to.  Format is a series of
 * pairs of lines (UNIX style), first the roomname and then the filename.
 * blank line signals end of file.
 */
char SendMapFile()
{
    int NormalRoomMap(SharedRoomData *room, int system, int roomslot, void *d);

    if (!ITL_Send(STARTUP)) return FALSE;

    EachSharedRoom(thisNet, NormalRoomMap, VirtualRoomMap, NULL);
    ITL_Line("");
    return ITL_Send(FINISH);
}

/*
 * NormalRoomMap()
 *
 * This sends the given room's name and cache file name if there is an
 * outgoing file pending or one will be built.
 */
static int NormalRoomMap(SharedRoomData *room, int system, int roomslot,
								void *d)
{
    char work[20];

    if (strLen(roomTab[roomslot].rtname) && HasOutgoing(room->room)) {
	ITL_Line(roomTab[roomslot].rtname);
	sprintf(work, CACHE_END_NAME, roomslot);
	ITL_Line(work);
    }
    return TRUE;
}

/*
 * ITL_Line()
 *
 * Sends the specified line to the SendITLchar function.
 */
void ITL_Line(char *data)
{
    while (*data) {
	sendITLchar((int) *data);
	data++;
    }
    sendITLchar((int) '\n');
}

/*
 * DelFile()
 *
 * This function kills the named file.
 */
void DelFile(DirEntry *f)
{
    unlink(f->unambig);
}

/*
 * MapFileAccepted()
 *
 * This function is responsible for discovering if the map file is acceptable.
 */
char MapFileAccepted()
{
    FILE *fd;
    char toReturn = TRUE, work[NAMESIZE + 5];

    ToTempArea();
    if (ITL_Receive("moo", FALSE, TRUE, putFLChar, fclose) == ITL_SUCCESS) {
	if ((fd = safeopen("moo", READ_ANY)) == NULL)
	    toReturn = FALSE;
	else {
	    GetAString(work, sizeof work, fd);
	    if (strLen(work) != 0) toReturn = FALSE;
	    fclose(fd);
	    unlink("moo");
	}
    }
    else toReturn = FALSE;
    KillTempArea();
    return toReturn;
}

#define TOP_COMP	4
/*
 * netFastTran()
 *
 * Accept an archive of files?
 */
void netFastTran(struct cmd_data *cmds)
{
	struct stat buf;
	PROTOCOL *External;
	char CheckMap(char *fn, char talk);
	int MTProtocol;
	char fn[150];
	char work[15];
	SYS_FILE MapFile;

	MTCompVal  = atoi(cmds->fields[0]);
	MTProtocol = atoi(cmds->fields[1]);

	if (RecMassTransfer || MTCompVal > TOP_COMP || MTCompVal < 0 ||
    			!DeCompAvailable(MTCompVal)) {
		reply(BAD, "nope");
		return;
	}

	MTProtocol = ReceiveProtocolValidate(MTProtocol, &External);

	if (MTProtocol == BAD_PROTOCOL) return;

	if (strCmp(cmds->fields[2], "0") != SAMESTRING ||
		strCmp(cmds->fields[3], "-1") != SAMESTRING) {
		reply(BAD, "");
		return;
	}

	reply(GOOD, "");

	makeSysName(MapFile, MAP_FILE, &cfg.netArea);
	if (ITL_Receive(MapFile,FALSE,TRUE,putFLChar,fclose) != ITL_SUCCESS) {
		return ;
	}

	KillList(&Mappings);
	if (CheckMap(MapFile, TRUE)) {
		splitF(netLog, "Accepting mass transfer %d\n", MTProtocol);

		if (receive(120) == NAK) return;

		sprintf(work, FAST_TEMPLATE, CompExtension(MTCompVal));
		ToTempArea();

		if (MTProtocol == DEFAULT_PROTOCOL) {
			if (ITL_Receive( work, FALSE,TRUE, putFLChar, fclose)
							!= ITL_SUCCESS) {
				KillTempArea();
				return ;
	    		}
		}
		else if (MTProtocol == ZM_PROTOCOL) {
			ExternalTransfer(External, work);
			while (MIReady()) Citinp();
		}

		if (stat(work, &buf) == 0) {
			reply(GOOD, "");
			/* recovery setup in case of power failure, etc */
			sprintf(fn, "%s%c%s%c%d", FAST_TRANS_FILE, I_SEP,
						TDirBuffer, I_SEP, MTCompVal);
			FastTemp = strdup(TDirBuffer);
			homeSpace();
			UpdateRecoveryFile(fn);
			RecMassTransfer = TRUE;
			splitF(netLog, "%ld bytes received\n", buf.st_size);
		}
		else {
			reply(BAD, "No file");
			KillTempArea();
			splitF(netLog, "File not received!\n");
			unlink(MapFile);
		}
	}
}

/*
 * ReceiveProtocolValidate()
 *
 * This function validates a protocol reception request.  We're limiting
 * ourselves to Zmodem or the default at the moment.
 */
int ReceiveProtocolValidate(int proto, PROTOCOL **External)
{
	if (proto == ZM_PROTOCOL) {
		if ((*External = FindProtocolByName("Zmodem", TRUE)) == NULL) {
			reply(BAD, "No protocol");
			return BAD_PROTOCOL;
		}
	}
	else if (proto != DEFAULT_PROTOCOL) {
		reply(BAD, "No protocol");
		return BAD_PROTOCOL;
	}
	return proto;
}
/*
 * CheckMap()
 *
 * Checks the map of room names to see if we refuse any.
 */
static char CheckMap(char *fn, char talk)
{
    FILE *fd;
    char work[2 * NAMESIZE];
    char toReturn = TRUE, bad;
    FileMap *data;
    RoomSearch arg;

    if (talk && !ITL_Send(STARTUP)) return FALSE;
    if ((fd = safeopen(fn, READ_ANY)) != NULL) {
	while (GetAString(work, sizeof work, fd) != NULL) {
	    bad = TRUE;
	    if (strLen(work) == 0) break;
	    if (strLen(work) < NAMESIZE) {
		strCpy(arg.Room, work);
		if (RoomRoutable(&arg)) {
		    bad = FALSE;
		    data = GetDynamic(sizeof *data);
		    data->RoomName = strdup(work);
		    GetAString(work, sizeof work, fd);
		    data->FileName = strdup(work);
		    AddData(&Mappings, data, NULL, FALSE);
		}
	    }

/* this form lets us respond to several errors with one chunk of code */
	    if (bad) {
		toReturn = FALSE;
		if (talk) ITL_Line(work);
		GetAString(work, sizeof work, fd);
	    }
	}
	fclose(fd);
    }
    else {
	if (talk) ITL_Line("moo");
	toReturn = FALSE;
    }
    if (talk) {
	ITL_Line("");
	ITL_Send(FINISH);
    }
    return toReturn;
}

/*
 * FreeMapping()
 *
 * Frees a mapping structure.
 */
static void FreeMapping(FileMap *data)
{
    free(data->FileName);
    free(data->RoomName);
    free(data);
}

/*
 * KillCacheFiles()
 *
 * This function kills all cache information concerning a node.  It is called
 * when a node is deleted from the nodelist, NOT when caching is simply turned
 * off.
 */
void KillCacheFiles(int which)
{
    char ArcFileName[100];

    MakeNetCacheName(ArcFileName, which);
	/* we check this due to the statement after - kill all files! */
    if (ChangeToCacheDir(ArcFileName) == 0)
	wildCard(DelFile, ALL_FILES, "", WC_NO_COMMENTS);
    homeSpace();
    rmdir(ArcFileName);
}

/*
 * ReadFastFiles()
 *
 * This function reads in all of the messages from an Arc file.
 */
void ReadFastFiles(char *dir)
{
	SYS_FILE fn;
	char work[15];
	void EatMsgFile();

	if (!RecMassTransfer) return;

	RecMassTransfer = FALSE;
	sprintf(work, FAST_TEMPLATE, CompExtension(MTCompVal));

	if (ChangeToCacheDir(dir) != 0) {
		splitF(netLog, "CTCD(%s) failure\n", dir);
		return;		/* ! */
	}
	strcpy(fn, work);

	NetDeCompress(MTCompVal, fn);

	homeSpace();

	RunListA(&Mappings, EatMsgFile, dir);

	if (ChangeToCacheDir(dir) != 0) {
		return;	/* or lose all in homeSpace */
	}

	unlink(fn);
	KillNetDeCompress(dir);

	makeSysName(fn, MAP_FILE, &cfg.netArea);
	unlink(fn);

	KillList(&Mappings);
	free(dir);
}

/*
 * EatMsgFile()
 *
 * This eats a message file extracted from an archive file.
 */
static void EatMsgFile(FileMap *data, char *dir)
{
    char fn[100];
    char vfn[100];
    int VirtNo;
    RoomSearch arg;

    strCpy(arg.Room, data->RoomName);
    if (!RoomRoutable(&arg)) {
	splitF(netLog, "Ooops - can't find %s\n", data->RoomName);
	return;
    }

    if (arg.virtual) {
	SetUpForVirtuals(arg.room->room, &VirtNo, vfn);
	MakeDeCompressedFilename(fn, data->FileName, dir);
	VirtualCopyFileToFile(fn, vfn);
    }
    else {
	MakeDeCompressedFilename(fn, data->FileName, dir);
	ReadNetRoomFile(arg.room->room, fn);
    }
}

/*
 * CacheMessages()
 *
 * This function is tasked with building cache files as needed.  We loop
 * through all the systems in the system list.  Those for which the
 * MassTransfer flag is active will cause the system to see if there are
 * any messages in the message base that are not in the cache for that
 * system.  If any are found, the messages are added to their respective
 * files (or create as necessary) and then the compression program is run for
 * that particular cache.  Flags must be set appropriately, of course.
 */
char CacheUpdated;
void CacheMessages(MULTI_NET_DATA whichNets, char VirtOnly)
{
    int rover;

    for (rover = 0; rover < cfg.netSize; rover++) {
	if ((netTab[rover].ntMemberNets & whichNets))
	    CacheSystem(rover, VirtOnly);
    }
}

/*
 * CacheSystem()
 *
 * This function caches a single system.  It is separated from CacheMessages()
 * so we can cache systems on an individual basis.
 */
void CacheSystem(int system, char VirtOnly)
{
    int CacheRoom(SharedRoomData *room, int system, int roomslot, void *d);
    char ArcFileName[60], Files[60], BaseName[40];

    if (!netTab[system].ntflags.MassTransfer) return;
    if (!gotCarrier()) DisableModem(TRUE);
    CacheUpdated = FALSE;
    EachSharedRoom(system, (VirtOnly) ? NULL : CacheRoom,
						CacheVirtualRoom, NULL);
    if (CacheUpdated) {
	putNet(system, &netBuf);
	if (CompAvailable(netBuf.nbCompress)) {
	    sprintf(BaseName, NETMSGS, CompExtension(netBuf.nbCompress));
	    NetCacheName(ArcFileName, system, BaseName);
	    NetCacheName(Files, system, CACHED_FILES);
	    Compress(netBuf.nbCompress, Files, ArcFileName);
	    if (access(ArcFileName, 0) != 0) {
		sprintf(msgBuf.mbtext, "Compress failed for %s?",
								netBuf.netName);
		splitF(netLog, "ERROR: %s\n", msgBuf.mbtext);
		aideMessage("Net Aide", FALSE);
	    }
	}
	UpdVirtStuff(TRUE);
    }
    UpdateSharedRooms();
    if (!gotCarrier()) EnableModem(TRUE);
}

/*
 * CacheRoom()
 *
 * This caches a room's messages as necessary.
 */
static int CacheRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
    extern char PrTransmit;
    char oldNet;
    int MsgCount;
    extern NetInfo NetStyle;
    char work[10], tempNm[3*NAMESIZE], commnd, doit, *name;

    if (roomTab[roomslot].rtlastMessage > room->room->lastMess &&
					strLen(roomTab[roomslot].rtname)) {
	if (thisNet != system) {
	    getNet(system, &netBuf);
	}

	zero_struct(NetStyle);
	Addressing(system, room->room, &commnd,&NetStyle.addr1, &NetStyle.addr2,
						&NetStyle.addr3, &name, &doit);

	NetStyle.sendfunc = putFLChar;
	PrTransmit = FALSE;
	sprintf(work, CACHE_END_NAME, roomslot);

	NetCacheName(tempNm, system, work);
	if ((upfd = fopen(tempNm, APPEND_ANY)) != NULL) {
	    oldNet = inNet;
	    inNet = NET_CACHE;
	    MsgCount = showMessages(NEWoNLY, room->room->lastMess, NetRoute);
	    inNet = oldNet;
	    fclose(upfd);
	    SetHighValues(room);
	    if (MsgCount == 0 && !GetFA(room->room->mode))
		unlink(tempNm);
	    else if (MsgCount != 0) {
		SetFA(room->room->mode);
		/* room->srd_flags |= SRD_DIRTY; set by SetHighValues */
		CacheUpdated = TRUE;
	    }
	}
	PrTransmit = TRUE;
    }
    return TRUE;
}

/*
 * RecoverMassTransfer()
 *
 * This function is charged with recovering a mass transfer file that was
 * not processed in the last net session due to a crash of some sort.
 * 
 * FORMAT: <mass id str><I_SEP><source dir><I_SEP><compression alg>
 */
void RecoverMassTransfer(char *line)
{
	SYS_FILE fn;
	char *colon;
	char *name;

	makeSysName(fn, MAP_FILE, &cfg.netArea);
	RecMassTransfer = TRUE;
	CheckMap(fn, FALSE);
	if ((colon = strchr(line, I_SEP)) == NULL) return;
	name = colon + 1;
	if ((colon = strchr(name, I_SEP)) == NULL) return;
	*colon = 0;
	MTCompVal = atoi(colon + 1);
	ReadFastFiles(strdup(name));
}

/*
 * UnCacheRoom()
 *
 * This turns off the cached flag for a given room shared thing.
 */
static int UnCacheRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
	if (strLen(roomTab[roomslot].rtname)) {
		UnSetFA(room->room->mode);
		room->srd_flags |= SRD_DIRTY;
		room->room->sr_flags |= SR_SENT;
	}
	return TRUE;
}
