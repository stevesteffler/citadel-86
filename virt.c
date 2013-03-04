/*
 *				virt.c
 *
 * Virtual room handler for Citadel-86.
 */

#include "ctdl.h"

/*
 *				history
 *
 * 92Jul23 HAW Tweaked for SharedRoom inclusion.
 * 91Nov15 HAW Restructured.
 * 88Apr01 HAW Final cleanup before release.
 */

/*
 *				contents
 *
 *	VirtInit()		Initialize the virtual room stuff
 *	InitVNode()		Init for a new node
 *	UpdVirtStuff()		Updates the virtual stuff
 *	VirtSummary()		Does real work of updating virtual stuff
 *
 */

VirtualRoom *VRoomTab;

void VirtSummary(char);

char VirtualInUse = FALSE;
int  VirtSize;

extern NetBuffer netBuf;
extern CONFIG    cfg;
extern int       thisNet;
extern FILE      *netLog;
extern char      netDebug, TrError;

/*
 * VirtInit()
 *
 * This function initializes the virtual room stuff, if available.  The virtual
 * room stuff is created by the virtadmn utility, so this is a non-fatal,
 * non-warning failure -- the sysop doesn't even know that this stuff ain't
 * here.
 */
void VirtInit()
{
#ifndef NO_VIRTUAL_ROOMS
    FILE *fd;
    SYS_FILE fn;
    long size;
    extern char *R_W_ANY;

    makeVASysName(fn, "ctdlvrm.sys");
    if ((fd = fopen(fn, R_W_ANY)) == NULL)
	return;	/* Depend on initializer to handle VirtualInUse */

    totalBytes(&size, fd);
    VRoomTab = (VirtualRoom *) GetDynamic((int) size);
    fread((char *) VRoomTab, (int) size, 1, fd);
    fclose(fd);
    VirtSize = (int) size / sizeof *VRoomTab;
    VirtualInUse = TRUE;
#endif
}

/*
 * InitVNode()
 *
 * When a new node is added to the net list, this function initializes the
 * virtual part of the new node.  This consists of enlarging the vnet table
 * size if necessary, and initializing the room pointers to -1, indicating
 * that none of them are in use.  Finally, the virtual tables on disk are
 * updated.
 */
void InitVNode(int slot)
{
#ifndef NO_VIRTUAL_ROOMS
#endif
}

/*
 * UpdVirtStuff()
 *
 * This function updates the virtual data on disk.
 */
void UpdVirtStuff(char SetOutGoing)
{
#ifndef NO_VIRTUAL_ROOMS
    FILE *fd;
    SYS_FILE fn;
    extern char *R_W_ANY;

    if (!VirtualInUse) return ;

    VirtSummary(SetOutGoing);

    makeVASysName(fn, "ctdlvrm.sys");
    if ((fd = fopen(fn, R_W_ANY)) == NULL)
	crashout("ctdlvrm.sys is missing!");

    fwrite(VRoomTab, VirtSize, sizeof *VRoomTab, fd);
    fclose(fd);
#endif
}

typedef struct {
    int target;
    char local;
} VirtArgs;

/*
 * VirtSummary()
 *
 * This function handles post-call cleanup.
 */
void VirtSummary(char SetOutGoing)
{
#ifndef NO_VIRTUAL_ROOMS
	int rover;
	VirtArgs args;
	int SetLastVirtualSent(SharedRoomData *room, int system, int roomslot,
								void *d);

	for (rover = 0; rover < VirtSize; rover++) {
		args.target = rover;
		if (VRoomTab[rover].vrChanged & LD_CHANGE) {
			VRoomTab[rover].vrHiLD++;
			args.local = FALSE;
			if (SetOutGoing)
				EachSharedRoom(thisNet, NULL,
						SetLastVirtualSent, &args);
		}
		if (VRoomTab[rover].vrChanged & LOC_CHANGE) {
			VRoomTab[rover].vrHiLocal++;
			args.local = TRUE;
		}
		VRoomTab[rover].vrChanged = 0;
	}
#endif
}

/*
 * SetLastVirtualSent()
 *
 * This function sets the last message value of a given shard room if it matches
 * with the target specified in the args argument.
 */
static int SetLastVirtualSent(SharedRoomData *room, int system, int roomslot,
								void *d)
{
    VirtArgs *args;

    args = d;
    if (roomslot == args->target) {
	if (args->local)
	    room->room->lastPeon = VRoomTab[roomslot].vrHiLocal;
	else
	    room->room->lastMess = VRoomTab[roomslot].vrHiLD;
	room->srd_flags |= SRD_DIRTY;
	return ERROR;
    }
    return TRUE;
}

