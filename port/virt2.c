/*
 *				virt2.c
 *
 * Virtual room handler for Citadel-86, part 2.
 */

#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"

/*
 *				history
 *
 * 92Jul23 HAW	Tweaked for SharedRoom inclusion.
 * 91Nov15 HAW	Restructured.
 * 89Apr04 HAW	Created.
 */

/*
 *				contents
 *
 *	VirtRoomRoutable()	Checks to see if a room is routable
 *	SendVirtual()		Sends a virtual room to another system
 *	RecVirtualRoom()	Receives a virtual room
 *	SetUpForVirtuals()	Gets set up for a virtual room reception
 *	DoVirtuals()		Go through all shared virtual rooms for a sys
 *	SendVirtualRoom()	Manages sending a virtual room out
 *	DumpVRoom()		Dumps a shared virtual room setup on console
 *	UnCacheVirtualRoom()	Unsets the cache bit
 *	VRNeedCall()		Check to see if there is outgoing for a room
 *	VirtualRoomOutgoing()	Same?
 *	VirtualRoomMap()	Sends the room/file map for a virtual room
 *	CacheVirtualRoom()	Caches a virtual room
 *
 */

extern VirtualRoom *VRoomTab;

extern char VirtualInUse;
extern int  VirtSize;

extern NetBuffer netBuf;
extern CONFIG    cfg;
extern int       thisNet;
extern FILE      *netLog;
extern char      inNet, netDebug, TrError;
extern char	 *READ_ANY, *APPEND_ANY;
extern FILE *upfd;
extern char *W_R_ANY;

char VNeedSend(SharedRoom *room, int roomslot, char NotPeon);
char VirtualOutgoingMessages(SharedRoom *room, int roomslot);
int ThrowAll(int which, char *distance, MSG_NUMBER start, MSG_NUMBER end,
						int (*SendFunc)(int c));
int SendVirtualNormalMessages(SharedRoomData *room, int roomslot,
						int (*SendFunc)(int c));

/*
 * VirtualOutgoingMessages()
 *
 * Are there outgoing messages, regardless of caching & room status?
 */
static char VirtualOutgoingMessages(SharedRoom *room, int roomslot)
{
    if (room->lastMess < VRoomTab[roomslot].vrHiLD || VGetFA(room->mode))
	return TRUE;

    if (VGetMode(room->mode) != PEON)
	if (room->lastPeon < VRoomTab[roomslot].vrHiLocal)
	    return TRUE;

    return FALSE;
}

/*
 * VirtRoomRoutable()
 *
 * This checks to see if the given room is the one we're looking for as
 * specified in the d parameter.
 */
int VirtRoomRoutable(SharedRoomData *room, int system, int roomslot, void *d)
{
    RoomSearch *arg;

    arg = d;
    if (strCmpU(VRoomTab[roomslot].vrName, arg->Room) == SAMESTRING) {
	arg->virtual = TRUE;
	arg->roomslot = roomslot;
	arg->room = room;
	arg->reason = FOUND;
	return ERROR;
    }
    return TRUE;
}

/*
 * SendVirtual()
 *
 * This function manages sending a room to another system.
 */
int SendVirtual(SharedRoomData *room)
{
    char	work[15];
    int		roomslot, count = 0;

    roomslot = netRoomSlot(room->room);

    if (VGetFA(room->room->mode)) {
	sprintf(work, V_CACHE_END_NAME, roomslot);
	if (SendPrepAsNormal(work, &count))
	    VUnSetFA(room->room->mode);
    }

    splitF(netLog, "Sending %s (virtual) ", VRoomTab[roomslot].vrName);
    count += SendVirtualNormalMessages(room, roomslot, sendITLchar);
    return count;
}

/*
 * SendVirtualNormalMessages()
 *
 * Sends normal virtual messages to some destination.
 */
static int SendVirtualNormalMessages(SharedRoomData *room, int roomslot,
						int (*SendFunc)(int c))
{
    int		count;
    MSG_NUMBER	StartMsg;

	/* Send all the new LD messages received */
    StartMsg = room->room->lastMess;
    count = ThrowAll(roomslot, LD_DIR, StartMsg, VRoomTab[roomslot].vrHiLD,
								SendFunc);
    if (TrError == TRAN_SUCCESS) {
	room->room->lastMess = VRoomTab[roomslot].vrHiLD;
	room->srd_flags |= SRD_DIRTY;
    }

    if (VGetMode(room->room->mode) != PEON) {
	StartMsg = room->room->lastPeon;
	count += ThrowAll(roomslot, LOCAL_DIR, StartMsg, 
					VRoomTab[roomslot].vrHiLocal, SendFunc);
	if (TrError == TRAN_SUCCESS) {
	    room->room->lastPeon = VRoomTab[roomslot].vrHiLocal;
	    room->srd_flags |= SRD_DIRTY;
	}
    }
    VRoomTab[roomslot].vrChanged |= SENT_DATA;
    return count;
}

/*
 * ThrowAll()
 *
 * This function sends a virtual room to another system.
 */
static int ThrowAll(int which, char *distance, MSG_NUMBER start, MSG_NUMBER end,
						int (*SendFunc)(int c))
{
#ifndef NO_VIRTUAL_ROOMS
	MSG_NUMBER  rover;
	int		count=0, slot;
	char	fn[100];
	extern FILE *netMisc;
	extern PROTO_TABLE Table[];
	extern char *R_SH_MARK, *LOC_NET, *NON_LOC_NET;
	extern int	TransProtocol;
	extern MessageBuffer   msgBuf;

	for (rover = start + 1; rover <= end; rover++) {
		CreateVAName(fn, which, distance, rover);
		if ((netMisc = fopen(fn, READ_ANY)) != NULL) {
			while (getMessage(getNetChar, TRUE, FALSE, TRUE)) {
/*
 * This gets around an ugly stupid bug of some sort involving fast
 * transfers.  120.678.
 */
				if ((slot = RoutePath(NON_LOC_NET, msgBuf.mbaddr)) == ERROR)
					if ((slot = RoutePath(LOC_NET, msgBuf.mbaddr)) == ERROR)
						if (isdigit(msgBuf.mbaddr[0]))
							slot = atoi(msgBuf.mbaddr);
				if (slot != thisNet) {
					msgBuf.mbaddr[0] = 0;
					count++;
					prNetStyle(TRUE, getNetChar, SendFunc,
								TRUE, "");
				}
			}
			fclose(netMisc);
		}
	}
	return count;
#else
	return 0;
#endif
}

/*
 * RecVirtualRoom()
 *
 * This function receives a virtual room directly from another system.
 */
int RecVirtualRoom(SharedRoomData *room, char ReplyFirst)
{
#ifndef NO_VIRTUAL_ROOMS
    int		roomslot;
    char	fn[50];
    extern long  TransferTotal;

    SetUpForVirtuals(room->room, &roomslot, fn);
    splitF(netLog, "Receiving %s (virtual) =>", VRoomTab[roomslot].vrName);
    if (ITL_StartRecMsgs(fn, ReplyFirst, TRUE, NULL) == ITL_SUCCESS) {
	splitF(netLog, " %ld bytes\n", TransferTotal);
	room->room->sr_flags |= SR_RECEIVED;
	return TRUE;
    }
    splitF(netLog, " Failed\n");
    return FALSE;
#else
    return TRUE;
#endif
}

/*
 * SetUpForVirtuals()
 *
 * This sets up a filename for a virtual room.
 */
void SetUpForVirtuals(SharedRoom *room, int *roomslot, char *fn)
{
    MSG_NUMBER  rover;
    char *distance;

    *roomslot = netRoomSlot(room);

    if (VGetMode(room->mode) != PEON) {
	distance = LD_DIR;
	rover = VRoomTab[*roomslot].vrHiLD + 1l;
	VRoomTab[*roomslot].vrChanged |= LD_CHANGE;
    }
    else {
	distance = LOCAL_DIR;
	rover = VRoomTab[*roomslot].vrHiLocal + 1l;
	VRoomTab[*roomslot].vrChanged |= LOC_CHANGE;
    }
    CreateVAName(fn, *roomslot, distance, rover);
}

char VirtualCheck(SharedRoomData *room, int roomslot, int *cmd);

/*
 * SendVirtualRoom()
 *
 * This function transfers a virtual room's contents to another system.
 */
int SendVirtualRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
	char		 doit;
	int		 *slot;
	int		 cmd;
	extern char	 MassTransferSent;
	SystemCallRecord *called;

	called = d;

	if (!gotCarrier()) return ERROR;

	if (SearchList(&called->SentVirtualRooms, &roomslot) != NULL) {
		return TRUE;	/* already sent, perhaps in another connect */
	}

	doit = VirtualCheck(room, roomslot, &cmd);
	if (doit && !MassTransferSent) {
		ITL_optimize(TRUE);
		if (findAndSend(cmd, room, SendVirtual,
				VRoomTab[roomslot].vrName, RecVirtualRoom)) {
			slot = GetDynamic(sizeof *slot);
			(*slot) = roomslot;
			AddData(&called->SentVirtualRooms, slot, NULL, FALSE);
		}
	}
	return TRUE;
}

/* #define VC_DEBUG */
/*
 * VirtualCheck()
 *
 * This handles deciding if a room should be sent.
 */
static char VirtualCheck(SharedRoomData *room, int roomslot, int *cmd)
{
    char doit;
    extern char inReceive;

    doit = TRUE;

    switch (VGetMode(room->room->mode)) {
    case PEON:
	*cmd = NET_ROOM;
	if (room->room->lastMess >= VRoomTab[roomslot].vrHiLD &&
			!VGetFA(room->room->mode)) {
	    doit = FALSE;
	}
	break;
    case BACKBONE:
	/* this is not clearly the right code */
	if ((VRoomTab[roomslot].vrChanged & SENT_DATA)) {
	    doit = FALSE;
	}
	else {
	    if (netBuf.nbflags.local &&
		room->room->lastMess >= VRoomTab[roomslot].vrHiLD &&
	        room->room->lastPeon >= VRoomTab[roomslot].vrHiLocal &&
			!VGetFA(room->room->mode)) {
		doit = FALSE;
	    }
	    else *cmd = (netBuf.nbflags.local) ? NET_ROOM : NET_ROUTE_ROOM;
	}
	break;
    default:
	splitF(netLog,"Error in virtuals for %s!\n", VRoomTab[roomslot].vrName);
	doit = ERROR;
    }
    return doit;
}

/*
 * VNeedSend()
 *
 * Determines if we need to send messages in a virtual regardless of the
 * mode, except we know it's a backbone situation (so check both local and
 * backbones).
 */
static char VNeedSend(SharedRoom *room, int roomslot, char NotPeon)
{
    return (room->lastMess < VRoomTab[roomslot].vrHiLD ||
	(NotPeon && room->lastPeon < VRoomTab[roomslot].vrHiLocal));
}

/*
 * FindVirtualRoom()
 *
 * This function finds the specified virtual room by name.
 */
int FindVirtualRoom(char *name)
{
    int rover;

    for (rover = 0; rover < VirtSize; rover++)
	if (strCmpU(VRoomTab[rover].vrName, name) == SAMESTRING)
	    return rover;
    return ERROR;
}

/*
 * DumpVRoom()
 *
 * This function dumps information concerning a virtual room.
 */
int DumpVRoom(SharedRoomData *room, int system, int roomslot, void *d)
{
#ifndef NO_VIRTUAL_ROOMS
    mPrintf("%s: ", VRoomTab[roomslot].vrName);
    switch (VGetMode(room->room->mode)) {
	case PEON: mPrintf("PEON"); break;
	case BACKBONE: mPrintf("Backbone"); break;
    }
    mPrintf(" (last bb sent %ld, hi bb %ld",
	room->room->lastMess, VRoomTab[roomslot].vrHiLD);
    if (VGetMode(room->room->mode) != PEON) {
	mPrintf(", last peon sent %ld, hi peon %ld",
	room->room->lastPeon, VRoomTab[roomslot].vrHiLocal);
    }
    mPrintf(")\n ");
    return TRUE;
#endif
}

/*
 * UnCacheVirtualRoom()
 *
 * Unset the flag indicating the room is cached for this system.
 */
int UnCacheVirtualRoom(SharedRoomData *room, int system, int which, void *d)
{
    VUnSetFA(room->room->mode);
    return TRUE;
}

/*
 * VRNeedCall()
 *
 * This discerns if the given virtual room has outgoing.
 */
int VRNeedCall(SharedRoomData *room, int system, int roomslot, char *d)
{
    if (VirtualOutgoingMessages(room->room, roomslot)) {
	*d = TRUE;
	return ERROR;
    }
    return TRUE;
}

/*
 * VirtualRoomOutgoing()
 *
 * This checks to see if the given virtual room has outgoing stuff, regardless
 * of room backbone status (we're already connected).
 */
int VirtualRoomOutgoing(SharedRoomData *room, int system, int roomslot,
								char *d)
{
    if (VirtualOutgoingMessages(room->room, roomslot)) {
	*d = TRUE;
	return ERROR;
    }
    return TRUE;
}

/*
 * VirtualRoomMap()
 *
 * This sends the name and cache file name of a virtual room.
 */
int VirtualRoomMap(SharedRoomData *room, int system, int roomslot, void *d)
{
    char work[20], NotPeon;

    NotPeon = (VGetMode(room->room->mode) != PEON);
    if (VGetFA(room->room->mode) || VNeedSend(room->room, roomslot, NotPeon)) {
	ITL_Line(VRoomTab[roomslot].vrName);
	sprintf(work, V_CACHE_END_NAME, roomslot);
	ITL_Line(work);
    }
    return TRUE;
}

/*
 * CacheVirtualRoom()
 *
 * This function should cache a virtual room.
 */
int CacheVirtualRoom(SharedRoomData *room, int sys, int roomslot, void *d)
{
    int		count;
    char	work[15], NotPeon;
    char	tempNm[3*NAMESIZE];
    extern char PrTransmit, CacheUpdated;

    PrTransmit = FALSE;
    NotPeon = (VGetMode(room->room->mode) != PEON);
    if (VNeedSend(room->room, roomslot, NotPeon)) {
	if (sys != thisNet)
	    getNet(sys, &netBuf);

	sprintf(work, V_CACHE_END_NAME, roomslot);
	NetCacheName(tempNm, thisNet, work);
	if ((upfd = fopen(tempNm, APPEND_ANY)) != NULL) {
	    count = SendVirtualNormalMessages(room, roomslot, putFLChar);
	    fclose(upfd);
	    if (count == 0 &&
		    !VGetFA(room->room->mode))
		unlink(tempNm);
	    else {
		VSetFA(room->room->mode);
		if (count) CacheUpdated = TRUE;
	    }
	}
    }
    PrTransmit = TRUE;
    return TRUE;
}

