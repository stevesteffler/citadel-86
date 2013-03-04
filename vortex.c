/*
 *				vortex.c
 *
 * Network "vortex" (aka infinite loop) handling code.
 */

/*
 *				history
 *
 * 88Oct23 HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	InitVortexing()		allocate & initialize buffers
 *	NotVortex()		checks for vortex possibility
 */

/*
 * Globals
 */

/*
 * This should only be accessed in this code, so we shan't put it in
 * CTDL.H.  It's not "really" a global structure.
 */
typedef struct {
	int	    vRoomNo;    /* Room we are associated with */
	AN_UNSIGNED vGen;       /* Allows checking for no longer in use */
	MSG_NUMBER  vHighest;   /* Highest msg # received */
	long	    vLastDate;  /* Date of last message received */
} VORTEX;

/*
 * This structure serves the following purposes:
 * 1. Contain the data for one (1) vortex detection structure (VORTEX data);
 * 2. Remember what slot in the associated .vtx file this particular VORTEX
 *    data occupies (int Slot);
 * 3. Remember the # of the highest message encountered associated with this
 *    slot, for later updating when processing for this room is finished
 *    (MSG_NUMBER Current);
 * 4. Mark this particular instantiation of this structure type as actually in
 *    use (used iff Current != 0l);
 * 5. Mark this particular instantiation as having actually detected a vortex
 *    (char Detect).  This is for Aide> reporting.
 */

typedef struct {
	int	   Slot, SystemNum;
	label	   System;
	MSG_NUMBER Current;
	long       DateCurrent;
	char       Detect;
	VORTEX     data;
} Vortex;

void *CheckVt();
void FreeVt();

/*
 * VortexList
 *
 * This is a list of systems found during a session of adding messages.  The
 * list is used for performance reasons.  When a session is over this list
 * is used to update our records on disk all at once..
 */
SListBase VortexList = { NULL, CheckVt, NULL, FreeVt, NULL };

char VortexHandle = FALSE;

/*
 * External variable definitions for NET.C
 */
extern CONFIG    cfg;	/* Lots an lots of variables    */
extern MessageBuffer   msgBuf;
extern rTable    *roomTab;
extern NetBuffer netTemp, netBuf;
extern FILE      *netLog;
extern int       thisRoom;
extern aRoom     roomBuf;	/* Room buffer	*/

/*
 * -Vortex Handling-
 *
 *    Vortex: the phenomenon of messages showing up on other systems more than
 * once.  Usually caused by a "loop" of backbones.
 *
 *    Vortexes can be caused by a number of problems, and the direct cause
 * of any particular vortex should not be important.  However, there is
 * a method available, as designed by CrT, to detect and nullify vortexes.
 *
 *    The method is to track the message numbers coming in for each system
 * sending messages.  Each message is supposed to carry the number it was
 * assigned on the originating system in one of the transmitted field.
 * Therefore, by remembering the highest message received from a given system,
 * we may identify and scuttle all messages which we believe we've already
 * received.
 *
 *    We must be aware that some shared rooms will be backboned, thus causing
 * messages not directly from the calling node to be sent to here.  We should
 * check these messages, too.
 *
 *    The central question is what level of granularity should be used to track
 * each system?  There are clearly two options:
 *
 * 1) Keep a single MSG_NUMBER for each system on the nodelist.  This has
 *    the attraction of little complexity -- the variable may be kept in
 *    the netTab & CTDLNET.SYS files, and will only take up 4 bytes per
 *    node.  However, this ignores the problem of interrupted net sessions:
 *    a room that was successfully transferred and processed can easily contain
 *    higher number messages than a room which was interrupted and
 *    re-transferred, thus causing incorrect vortex detection.  Interrupted
 *    sessions, while certainly not the rule, are not uncommon, thus making
 *    this objection reasonable.  Additionally, implementation of this option
 *    would require a Major Release, which is not appealing at the moment.
 *
 * 2) Keep a MSG_NUMBER for each room that each system nets with.  This option
 *    is clearly far more complex, but it does solve the major problem of
 *    option 1.  Furthermore, it should not require a Major Release in order
 *    to implement, which makes it more attractive.  Unfortunately, it may
 *    require some rewriting of the getNet code in order to support multiple
 *    netBuf variables, but that should not be utterly ghastly (or so we
 *    pray).
 *
 *    Neither option handles virtual rooms, nor are they intended to.  At the
 * moment, such support is not seen as warranted.
 *
 *    The initial implementation of net vortexing will be of option 2).  If, at
 * a later date, it becomes clear that 2) is unworkable or unneeded for some
 * reason, we can switch to Option 1) easily at the next Major Release point.
 *
 * IMPLEMENTATION
 *
 *    In order to avoid a Major Release, we cannot change the structure of
 * CTDLNET.SYS.  Furthermore, it is not attractive to attempt to handle an
 * arbitrary number of shared rooms per node within CTDLNET.SYS -- if we attempt
 * to handle the problem in the same manner as shared rooms and net archive
 * rooms (not implemented as of yet, sigh), we will run into limits, perhaps
 * far too often.
 *
 *    Therefore, we shall handle vortex records in the following way.
 *
 * 1) Each node on a system's nodelist may or may not have a file named
 *    "#.vtx", where "#" is the index into CTDLNET.SYS for that record.  These
 *    files will be created as needed, and thus some or many nodes (in
 *    particular disabled nodes) will not have corresponding .vtx files.  All
 *    .vtx files will be located in the #NETAREA directory.
 *
 * 2) Any .vtx file will be composed of 1 or more records of type VORTEX.
 *    Refer to CTDL.H for details, but each record is made up of an integer
 *    associated with roomTab, a GenNumber so we can detect when this record
 *    is no longer really in use, and the highest Message # received.
 *
 * 3) C-86 will keep an internal list of VORTEX records, one per node, during
 *    net processing.  As a room is processed, each message will force a
 *    #.vtx access when the originating system is occuring for the first
 *    time during this processing.  If the .vtx file contains a record for
 *    this room, then it is loaded into the internal list and can be referenced
 *    for the balance of this room's processing without going to disk.  If
 *    no record is found or the #.vtx file does not exist, appropriate creation
 *    takes place.  If any message is found to be less than the last highest
 *    received for this room, it is discarded.  [An audit would probably be
 *    desirable, too.]
 *
 *    Please note that no #.vtx files will be created or utilized if the option
 * has not been enabled for this installation.  Currently, this option is
 * enabled by placing "+vortex" on the CTDL command line.  Only systems with
 * excess disk capacity should use this capability.
 */

/*

   88Oct19 from Thom Brown @ Utica College, NY
  
 Hue - about the new code that detects (and presumably rejects) vortexed 
messages.  Would it be possible to have Citadel post a message to the Aide 
  
 I have this fear that now that we don't get vortexed messages there will be no 
attempt for offending systems to clean up their configurations, so to speak. 
  
 I suppose a parallel message out to go back to the offending system as well. 

*/

/*
 * 90July
 *
 * The scheme mentioned above is now being rewritten to function independent
 * of CtdlNet.Sys.  A database will be constructed as new systems are
 * detected.
 */

#define getVortex(x, y)		(fread(y, sizeof *(y), 1, x) == 1) 
#define putVortex(x, y)		fwrite(y, sizeof *(y), 1, x)

typedef struct {
	label Id, Name;
	char InUse;
} VtRecord;

static UNS_16 *VI;
static UNS_16 TopVx = 0;

extern char *READ_ANY, *APPEND_ANY;
extern char *R_W_ANY, *WRITE_ANY;
int searchVortex(char *id, VtRecord *VtSystem);

/*
 * VortexInit()
 *
 * This will initialize vortexing once and only once while an installation
 * is up.  It creates a dynamic table from the contents of vtxind.sys, an
 * index table.
 */
void VortexInit()
{
    FILE *fd;
    SYS_FILE name;
    long bytes;

    if (!VortexHandle) return ;
    makeSysName(name, "vtxind.sys", &cfg.netArea);
    if ((fd = fopen(name, READ_ANY)) == NULL) VI = NULL;
    else {
	totalBytes(&bytes, fd);
	VI = GetDynamic((int) bytes);
	if (fread(VI, (int) bytes, 1, fd) != 1)
	    VI = NULL;
	else
	    TopVx = (int) bytes/sizeof(*VI);
	fclose(fd);
    }
}

/*
 * InitVortexing()
 *
 * This is an initialization function which should be called before each
 * session of checking messages in from a network session.  Although at
 * the moment it does nothing, it and the call to it should be retained
 * in the interests of completeness and convenience in case something needs
 * to be added at a later date.
 */
void InitVortexing()
{
}

/*
 * NotVortex()
 *
 * This checks to see if the msg in msgBuf is vortexing or not.  FALSE is
 * returned if the message should be discarded.
 */
char NotVortex()
{
    int		NotUsed = -1, slot;
    char	found, *low, IdAvailable = FALSE, DateAvailable = FALSE;
    label       temp;
    SYS_FILE    vortex;
    FILE	*fd;
    MSG_NUMBER  srcId = 0l;
    long	srcDate = 0l;
    extern char *READ_ANY;
    Vortex      *Vtx;
    VtRecord    VtSystem;

    /* If no vortexing or we can't identify the nodeId ... */
    if (!VortexHandle || !normId(msgBuf.mborig, temp))
	return TRUE;

    /*
     * If system not in vortexlist, then we must add it first before continuing
     * onwards.
     */
    if ((Vtx = SearchList(&VortexList, temp)) == NULL) {
	if ((slot = searchVortex(temp, &VtSystem)) == ERROR) {
	    splitF(netLog, "Msg from unmonitored node %s @%s, adding it.\n",
					msgBuf.mboname, msgBuf.mborig);
		/* adding new element to list */
	    if (TopVx == 0)
		VI = malloc(++TopVx * sizeof *VI);
	    else
		VI = realloc(VI, ++TopVx * sizeof *VI);

	    VI[TopVx - 1] = hash(temp);
	    makeSysName(vortex, "vtxind.sys", &cfg.netArea);
	    if ((fd = fopen(vortex, WRITE_ANY)) == NULL) {
		printf("Problems with vortex index file (%s, errno %d).\n",
								vortex, errno);
		return FALSE;
	    }
	    fwrite(VI, 1, TopVx * sizeof *VI, fd);
	    fclose(fd);

	    strCpy(VtSystem.Id, temp);
	    strCpy(VtSystem.Name, msgBuf.mboname);
	    VtSystem.InUse = TRUE;
	    makeSysName(vortex, "vortex.sys", &cfg.netArea);
	    if ((fd = fopen(vortex, (TopVx == 1) ? WRITE_ANY : APPEND_ANY))
								== NULL) {
		printf("Problems with vortex file.\n");
		return FALSE;
	    }
	    fwrite(&VtSystem, 1, sizeof VtSystem, fd);
	    fclose(fd);
	    slot = TopVx - 1;
	}
    }
    else slot = Vtx->SystemNum;

    if (Vtx == NULL) {
	/*
	 * Check to see if this file exists
	 */
	Vtx = GetDynamic(sizeof *Vtx);
	AddData(&VortexList, Vtx, NULL, FALSE);
	strCpy(Vtx->System, temp);
	Vtx->SystemNum = slot;
	sprintf(temp, "%d.vex", slot);
	makeSysName(vortex, temp, &cfg.netArea);
	if ((fd = fopen(vortex, READ_ANY)) != NULL) {
	    /*
	     * File is here, so search for a record corresponding to this room.
	     */
	    Vtx->Slot = 0;
	    while ((found = getVortex(fd, &Vtx->data))) {

		/* First, keep an eye out for recycling */
		if (Vtx->data.vGen !=
			roomTab[Vtx->data.vRoomNo].rtgen)
		    NotUsed = Vtx->Slot;      /* mark it */
		else if (Vtx->data.vRoomNo == thisRoom)
		    break;      /* Found it, so stop here */
		Vtx->Slot++;
	    }

	    if (!found) {
		/* Garbage collection measure */
		if (NotUsed != -1)
		    Vtx->Slot = NotUsed;
	    }
	    fclose(fd);
	}

	if (fd == NULL)
	    Vtx->Slot = 0;

	if (fd == NULL || !found) {
	    Vtx->data.vRoomNo = thisRoom;
	    Vtx->data.vGen = roomTab[thisRoom].rtgen;
	    /* this shouldn't hurt */
	    Vtx->data.vHighest = 1l;
	}
    }

    /*-Construct srcId number here-*/
    if (strLen(msgBuf.mbsrcId) != 0) {
	IdAvailable = TRUE;
	if ((low = strchr(msgBuf.mbsrcId, ' ')) == NULL)
	    srcId = atol(msgBuf.mbsrcId);
	else
	    srcId = (atol(msgBuf.mbsrcId) << 16) + atol(low + 1);
    }

    if (strLen(msgBuf.mbdate) != 0 || strLen(msgBuf.mbtime) != 0)
	if ((DateAvailable = ReadDate(msgBuf.mbdate, &srcDate)))
	    srcDate += ReadTime(msgBuf.mbtime);

   /*
    * Comparison note:
    * The current scheme involves both the date of the message and the
    * source ID of the message.  This gives us security against both
    * incorrect system dates and message base resetting.  Unfortunately,
    * STadel does not transmit source IDs.  Therefore, the rules for
    * comparison are a little obscure.  Basically, the following if
    * boils down to this:
    *  o if the source id is missing, compare against the latest date
    *    of a message received in this room for the source system of
    *    this message.  The comparison is NON-inclusive (i.e., strictly
    *    less than), because it is conceivable for two net messages to
    *    contain the same date and time -- message composition is not
    *    always a lengthy process.  Note this leaves a "hole" in the
    *    vortex detection department -- we don't dare test non-strictly,
    *    since that can result in losing net messages, but this lets
    *    certain vortexed messages get through.
    *  o if both date and source id are available, then the message
    *    must fail both tests before it is rejected.  That's our
    *    fail-safe mentioned earlier.  During this* comparison, we
    *    use an inclusive (non-strict) comparison of message dates,
    *    because the message # check will take care of distinguishing
    *    between two messages written under the same date/time stamp.
    *    Therefore, there's no hole for us to worry about.  That
    *    difference is what makes this if so scary looking.
    */
    if ((!IdAvailable || srcId <= Vtx->data.vHighest) &&
	(!DateAvailable || (IdAvailable && srcDate <= Vtx->data.vLastDate) ||
 			   (!IdAvailable && srcDate < Vtx->data.vLastDate))) {
	splitF(netLog, "%s from %s rejected.\n", msgBuf.mbsrcId,
						msgBuf.mboname);
	Vtx->Detect = TRUE;

	return FALSE;
    }
    else {
	if (IdAvailable)
	    Vtx->Current = max(Vtx->Current, srcId);
	if (DateAvailable)
	    Vtx->DateCurrent = max(Vtx->DateCurrent, srcDate);
	return TRUE;
    }
}

static int errors;
/*
 * FinVortexing()
 *
 * This function should be called to finish a vortex checking session.  This
 * saves updates to disk.
 */
void FinVortexing()
{
    if (!VortexHandle)
	return;

    sprintf(msgBuf.mbtext, "Vortex: Attempted by %s in %s, involving ",
					netBuf.netName, roomBuf.rbname);
    errors = 0;

    KillList(&VortexList);

    if (errors) {
	strCat(msgBuf.mbtext, ".");
	netResult(msgBuf.mbtext);
    }
}

/*
 * FreeVt()
 *
 * This will write and free an element of the vortex list; obviously, it is
 * used in list handling.
 */
static void FreeVt(Vortex *d)
{
    label    temp;
    SYS_FILE vortex;
    FILE     *fd;
    VtRecord VtSystem;

    if (d->Current > d->data.vHighest)
	d->data.vHighest = d->Current;

    if (d->DateCurrent > d->data.vLastDate)
	d->data.vLastDate = d->DateCurrent;

    sprintf(temp, "%d.vex", d->SystemNum);
    makeSysName(vortex, temp, &cfg.netArea);
    if ((fd = fopen(vortex, R_W_ANY)) == NULL) {
	if ((fd = fopen(vortex, WRITE_ANY)) == NULL) {
	    splitF(netLog, "Couldn't create %s!!\n", vortex);
	}
    }
    else {
	fseek(fd, d->Slot * sizeof d->data, 0);
    }

    if (fd != NULL) {
	putVortex(fd, &d->data);
	fclose(fd);
    }

    if (d->Detect) {
	errors++;
	if (searchVortex(d->System, &VtSystem) != ERROR)
	    sprintf(lbyte(msgBuf.mbtext), (errors == 1) ? "%s" : ", %s",
						VtSystem.Name);
    }
    free(d);
}

/*
 * CheckVt()
 *
 * This function is used to search the list of vortex records for a given
 * system.
 */
void *CheckVt(Vortex *d, char *s)
{
    return (strCmpU(d->System, s) == SAMESTRING) ? d : NULL;
}

/*
 * ReadTime()
 *
 * This function reads the time stamp of a message and returns a long
 * integer indicating the absolute time of the message in seconds past
 * midnight.  It does NOT handle time zones.
 */
long ReadTime(char *time)
{
    char *s, afternoon;
    long ret;

    if (strLen(time) == 0) return 0l;

    if ((s = strrchr(time, ' ')) == NULL) return 0l;

    if (strCmpU(s + 1, "am") == SAMESTRING) afternoon = FALSE;
    else if (strCmpU(s + 1, "pm") == SAMESTRING) afternoon = TRUE;
    else return 0l;

    if ((s = strchr(time, ':')) == NULL) return 0l;

    ret = atol(time) * 60l;
    if (afternoon) {
	if (atol(time) != 12) ret += 720l;
    }
    else if (atol(time) == 12) ret = 0l;

    ret += atol(s + 1);
    return 60 * ret;
}

/*
 * searchVortex()
 *
 * This function will search the vortex list for the given system.  It
 * return ERROR if the system is not in the database, otherwise an index
 * into it in terms of records.
 */
static int searchVortex(char *id, VtRecord *VtSystem)
{
    int      rover;
    UNS_16   hval;
    FILE     *fd = NULL;
    SYS_FILE name;

    makeSysName(name, "vortex.sys", &cfg.netArea);
    if ((fd = fopen(name, READ_ANY)) == NULL) return ERROR;
    hval = hash(id);
    for (rover = 0; rover < TopVx; rover++) {
	if (VI[rover] == hval) {
	    if (fd == NULL)
		if ((fd = fopen(name, READ_ANY)) == NULL) return ERROR;
	    fseek(fd, rover * sizeof *VtSystem, 0);
	    if (fread(VtSystem, sizeof *VtSystem, 1, fd) > 0) {
		if (strCmpU(id, VtSystem->Id) == SAMESTRING) break;
	    }
	}
    }
    if (fd != NULL)
    	fclose(fd);
    return (rover == TopVx) ? ERROR : rover;
}
