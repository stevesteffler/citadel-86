/*
 *				shared.c
 *
 * shared room functions.
 */

/*
 *				history
 *
 * 92Jul19 HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 */

static int SR_Count = 0;
static FILE *sharedfd;

extern CONFIG    cfg;            /* Lots an lots of variables    */
extern NetTable  *netTab;
extern rTable    *roomTab;

void *FindShared();
SListBase SharedRooms = { NULL, FindShared, NULL, free, NULL };

/*
 * initSharedRooms()
 *
 * This function initializes the shared rooms of the system.  It reads the
 * shared room information from shared.sys and places it in the internal list
 * SharedRooms, where each record is not a SharedRoom, but a SharedRoomData.
 * This data type includes a slot number and a flags field including a dirty
 * bit-flag, which indicates if the given record needs to be written.
 *
 * This list is periodically written out as needed, so we are basically
 * mirroring the the disk list with a RAM list.
 */
void initSharedRooms(char check)
{
    SharedRoom *room;
    SharedRoomData *roomd;
    SYS_FILE shared;

    makeSysName(shared, "shared.sys", &cfg.netArea);
    if ((sharedfd = fopen(shared, READ_ANY)) != NULL) {
	room = GetDynamic(sizeof *room);
	while (fread(room, sizeof *room, 1, sharedfd) > 0) {
	    roomd = GetDynamic(sizeof *roomd);
	    roomd->slot = SR_Count++;
	    roomd->srd_flags = 0;
	    roomd->room = room;
	    AddData(&SharedRooms, roomd, NULL, TRUE);
	    room = GetDynamic(sizeof *room);
	}
	free(room);
	fclose(sharedfd);
    }
}

/*
 * UpdateSharedRooms()
 *
 * This function updates the disk version of the shared rooms with dirty records
 * in the RAM list.
 */
void UpdateSharedRooms()
{
    char flag;
    SYS_FILE shared;
    void UpdateSR(), FindDirty();

    flag = FALSE;
    RunListA(&SharedRooms, FindDirty, &flag);
    if (flag) {
	makeSysName(shared, "shared.sys", &cfg.netArea);
	if ((sharedfd = fopen(shared, R_W_ANY)) == NULL) {
	    if ((sharedfd = fopen(shared, WRITE_ANY)) == NULL) {
		printf("CAN'T UPDATE SHARED ROOM FILE!\n");
		return;
	    }
	}
	RunList(&SharedRooms, UpdateSR);
	fclose(sharedfd);
    }
}

/*
 * FindDirty()
 *
 * This function sets the passed flag to true if the record is dirty.
 */
static void FindDirty(SharedRoomData *room, char *flag)
{
    if (room->srd_flags & SRD_DIRTY)
	*flag = TRUE;
}

/*
 * UpdateSR()
 *
 * This function updates a record in the shared.sys file on disk.
 */
static void UpdateSR(SharedRoomData *room)
{
    if (room->srd_flags & SRD_DIRTY) {
	fseek(sharedfd, sizeof(*room->room) * room->slot, 0);
	fwrite(room->room, sizeof *room->room, 1, sharedfd);
	room->srd_flags &= ~SRD_DIRTY;
    }
}

typedef struct {
    int  system;
    int  (*func)(SharedRoomData *room, int system, int roomslot, void *d);
    int  (*virtfunc)(SharedRoomData *room, int system, int roomslot, void *d);
    int  flags;
    void *data;
    char result;
} SrFuncs;

/*
 * EachSharedRoom()
 *
 * This does something for each shared room.
 */
void EachSharedRoom(int system,
    int (*func)(SharedRoomData *room, int system, int roomslot, void *d),
    int (*virtfunc)(SharedRoomData *room, int system, int roomslot, void *d),
    void *data)
{
    void ExecFunc();
    SrFuncs Funcs; 

    if (!netTab[system].ntflags.in_use) return;

    Funcs.system = system;
    Funcs.func = func;
    Funcs.virtfunc = virtfunc;
    Funcs.data = data;
    Funcs.result = FALSE;

    RunListA(&SharedRooms, ExecFunc, &Funcs);
}

/*
 * ExecFunc()
 *
 * This is a work function used as an iterator to go through the
 * SharedRooms list.  For each element the function given to EachSharedRoom()
 * is executed (via the funcs argument to this).  If it returns ERROR then
 * the result field of funcs argument is set and checked on following
 * iterations, so all further processing can be stopped if necessary.
 */
static void ExecFunc(SharedRoomData *room, SrFuncs *funcs)
{
    int roomslot;
    extern int VirtSize;
    extern VirtualRoom *VRoomTab;

    if (funcs->result || (room->room->sr_flags & SR_NOTINUSE)) return;

    roomslot = netRoomSlot(room->room);

    if (room->room->netSlot == funcs->system) {
	if (!(room->room->sr_flags & SR_VIRTUAL)) {
	    if (funcs->func != NULL && roomValidate(room->room)) {
		if ((*funcs->func)(room, funcs->system, roomslot,
							funcs->data) == ERROR)
		    funcs->result = TRUE;
	    }
	}
	else {
	    if (funcs->virtfunc != NULL && roomslot < VirtSize &&
				roomslot >= 0 && VRoomInuse(roomslot)) {
		if ((*funcs->virtfunc)(room, funcs->system, roomslot,
							funcs->data)==ERROR)
		    funcs->result = TRUE;
	    }
	}
    }
}

/*
 * Clean()
 *
 * This function turns off the flag indicating data has been received for
 * this room.
 */
void Clean(SharedRoomData *room)
{
    room->room->sr_flags &= ~SR_RECEIVED;
}

/*
 * NewSharedRoom()
 *
 * This function finds a slot for a new shared room.
 */
SharedRoomData *NewSharedRoom()
{
    SharedRoomData *Found = NULL;
    void FindUnused();

    RunListA(&SharedRooms, FindUnused, &Found);
    if (Found == NULL) {
	Found = GetDynamic(sizeof *Found);
	Found->room = GetDynamic(sizeof *Found->room);
	Found->slot = SR_Count++;
	AddData(&SharedRooms, Found, NULL, FALSE);
    }
    Found->srd_flags = SRD_DIRTY;
    return Found;
}

/*
 * FindUnused()
 *
 * This function checks a slot to see if it's in use.
 */
static void FindUnused(SharedRoomData *room, SharedRoomData **Found)
{
    if (*Found == NULL && (room->room->sr_flags & SR_NOTINUSE)) {
	*Found = room;
    }
}

/*
 * KillSharedRoom()
 *
 * This function kills all shared rooms records associated with the specified
 * room.
 */
void KillSharedRoom(int room)
{
    void KillRecord();

    RunListA(&SharedRooms, KillRecord, &room);
    UpdateSharedRooms();
}

/*
 * KillRecord()
 *
 * This function kills a record if it matches the roomslot specified and it's
 * not virtual.  Virtual room kills currently only take place in a utility.
 */
static void KillRecord(SharedRoomData *room, int *roomslot)
{
    if (!(room->room->sr_flags & SR_NOTINUSE) &&
		!(room->room->sr_flags & SR_VIRTUAL) &&
			*roomslot == netRoomSlot(room->room)) {
	room->room->sr_flags |= SR_NOTINUSE;
    }
}

void *FindShared(SharedRoomData *roomd, SharedRoomData *new)
{
    if (!(roomd->room->sr_flags & SR_NOTINUSE) &&
        !(new->room->sr_flags & SR_NOTINUSE) &&
	(roomd->room->sr_flags & SR_VIRTUAL) ==
	(new->room->sr_flags & SR_VIRTUAL) &&
	roomd->room->netSlot == new->room->netSlot &&
	roomd->room->netGen == new->room->netGen &&
	roomd->room->srslot == new->room->srslot &&
	roomd->room->srgen == new->room->srgen) {
	new->room->sr_flags |= SR_NOTINUSE;
	new->srd_flags |= SRD_DIRTY;
    }

    return NULL;
}

/*
 * KillShared()
 *
 * This function is responsible for killing the specified shared room record.
 */
int KillShared(SharedRoomData *room, int system, int roomslot, void *d)
{
    room->room->sr_flags |= SR_NOTINUSE;
    room->srd_flags |= SRD_DIRTY;
    return TRUE;
}

/*
 * EachSharingNode
 *
 * This function iterates through all nodes sharing the given room.
 */
void EachSharingNode(int room, int flags,
    int (*func)(SharedRoomData *room, int system, int roomslot, void *d),
    void *data)
{
	SrFuncs Funcs; 
	void RoomExecFunc();

	if (!roomTab[room].rtflags.INUSE)
		return;

	Funcs.system = room;
	Funcs.func   = func;
	Funcs.data   = data;
	Funcs.result = FALSE;
	Funcs.flags  = flags;

	RunListA(&SharedRooms, RoomExecFunc, &Funcs);
}

static void RoomExecFunc(SharedRoomData *room, SrFuncs *funcs)
{
	int roomslot;
	extern int VirtSize;
	extern VirtualRoom *VRoomTab;

	if (funcs->result || (room->room->sr_flags & SR_NOTINUSE)) return;

	roomslot = netRoomSlot(room->room);

	if (roomslot == funcs->system &&
		( ((room->room->sr_flags & SR_VIRTUAL) &&
                     (funcs->flags & SR_VIRTUAL)) ||
	          (!(room->room->sr_flags & SR_VIRTUAL) &&
                     !(funcs->flags & SR_VIRTUAL)))) {
		if ((!(funcs->flags & SR_VIRTUAL) &&
				roomValidate(room->room)) ||
			((funcs->flags & SR_VIRTUAL) && roomslot < VirtSize &&
				roomslot >= 0 && VRoomInuse(roomslot))) {
			if ((*funcs->func)(room, room->room->netSlot, roomslot,
							funcs->data) == ERROR)
			funcs->result = TRUE;
		}
	}
}
