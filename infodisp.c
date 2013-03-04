/*
 *				InfoDisp.C (try #2)
 *
 * Handles display aspects of the Information command of C-86.
 */

#include "ctdl.h"

/*
 *				history
 *
 * 92May28 HAW	Created.
 */

/*
 *				Contents
 *
 *	EditInfo()		Adds new information.
 *	GetInfo()		Get information for a room.
 *	doInfo()		Dump info on room.
 *	KillInfo()		Kills info of dead room.
 *	AllInfo()		.Known Info.
 *	InfoShow()		work fn to show information.
 */

extern CONFIG  cfg;
extern struct floor  *FloorTab;
extern rTable    *roomTab;
extern MessageBuffer msgBuf;
extern logBuffer logBuf;
extern aRoom   roomBuf;		/* Room buffer			*/
extern char    haveCarrier;	/* Have carrier?		*/
extern char    onConsole;	/* How about on Console?	*/
extern char    outFlag;		/* Output flag			*/
extern int     thisRoom;

extern SListBase InfoMap;


/*
 * EditInfo()
 *
 * This function adds new information.  Once the new information is acquired
 * the entire list is written back out to disk.
 */
void EditInfo()
{
    extern FILE *upfd;
    extern char EndWithCR;
    int  HiNumber = 0;
    void FindHighest();
    NumToString *map;
    char temp[20];
    SYS_FILE name;

    msgBuf.mbtext[0] = 0;
    if ((map = SearchList(&InfoMap, roomBuf.rbname)) != NULL) {
	if (getYesNo("Edit current info")) {
	    GetInfo(roomBuf.rbname);
	}
    }

    mPrintf("\n Information editing");
    doCR();
    CleanEnd(msgBuf.mbtext);
    mPrintf("%s", msgBuf.mbtext);
    outFlag = OUTOK;
    if (GetBalance(ASCII,msgBuf.mbtext,MAXTEXT-50,INFO_ENTRY,"") && onLine()) {
	CleanEnd(msgBuf.mbtext);
	if (map != NULL) {
	    sprintf(temp, "%d.inf", map->num);
	}
	else {
	    sprintf(temp, "%d.inf", thisRoom);
	    makeSysName(name, temp, &cfg.infoArea);
	    if (access(name, 0) == 0) {
		HiNumber = 0;
		RunListA(&InfoMap, FindHighest, (void *) &HiNumber);
		HiNumber++;
		sprintf(temp, "%d.inf", HiNumber);
	    }
	    else HiNumber = thisRoom;
	}
	makeSysName(name, temp, &cfg.infoArea);
	redirect(name, INPLACE_OF);
	mFormat(msgBuf.mbtext, oChar, doCR);
	fprintf(upfd, "\n%s\n", END_INFO);
	undirect();
	if (map == NULL) {
	    AddData(&InfoMap, NtoStrInit(HiNumber, roomBuf.rbname, 0, FALSE),
						NULL, FALSE);
	    WriteOutInformation();
	}
    }
}

/*
 * FindHighest()
 *
 * Silly find an empty spot function.
 */
static void FindHighest(NumToString *element, int *HiNumber)
{
	if (element->num > *HiNumber) *HiNumber = element->num;
}

/*
 * GetInfo()
 *
 * This function will get the information for a given room.
 */
static char *GetInfo(label name)
{
    char line[100];
    NumToString *map;
    char temp[20], *cheat;
    SYS_FILE sysname;
    FILE *fd;

    if ((map = SearchList(&InfoMap, name)) != NULL) {
	sprintf(temp, "%d.inf", map->num);
	msgBuf.mbtext[0] = 0;
	cheat = msgBuf.mbtext;
	makeSysName(sysname, temp, &cfg.infoArea);
	if ((fd = fopen(sysname, READ_TEXT)) != NULL) {
	    while (fgets(line, sizeof line, fd) != NULL) {
		if (strncmp(line, END_INFO, strlen(END_INFO)) == 0) break;
		strcat(cheat, line);
		cheat = cheat + strlen(cheat);
	    }
	    fclose(fd);
	    return strdup(msgBuf.mbtext);
	}
	return NULL;
    }
    return NULL;
}

/*
 * doInfo()
 *
 * This function dumps info on current room.
 */
char doInfo()
{
    char *c;
    extern char journalMessage;
    extern int	 CurLine;

    mPrintf("\n \n ");
    if ((c = GetInfo(roomBuf.rbname)) != NULL) {
	CurLine = 1;
	PagingOn();
	mPrintf("%s", c);
	PagingOff();
	doCR();
	if (journalMessage) {
	    if (redirect(NULL, APPEND_TO)) {
		mPrintf("%s", c);
		doCR();
		undirect();
	    }
	    journalMessage = FALSE;
	}
	free(c);
    }
    else mPrintf("No information available about %s.\n ", roomBuf.rbname);
    return GOOD_SELECT;
}

/*
 * ChangeInfoName()
 *
 * This function is called when a room name is changed.
 */
void ChangeInfoName(char *newname)
{
    NumToString *map;

    if ((map = SearchList(&InfoMap, roomBuf.rbname)) != NULL) {
	free(map->string);
	map->string = strdup(newname);
	WriteOutInformation();
    }
}

/*
 * AllInfo()
 *
 * This function implements .Known Info.
 */
void AllInfo()
{
    int rover, roomNo;
    extern int TopFloor;
    int InfoShow();
    extern char ShowNew, SelNew;
    extern int	 CurLine;

    doCR();
    PagingOn();
    if (FloorMode) {
	for (rover = 0; rover < TopFloor; rover++) {
	    roomNo = FirstRoom(rover);
	    ShowNew = 2;
	    SelNew = TRUE;
	    if (FloorRunner(roomNo, CheckFloor) != ERROR) {
		mPrintf("[%s]", FloorTab[rover].FlName);
		doCR();
		FloorRunner(roomNo, InfoShow);
	    }
	    ShowNew = FALSE;
	    SelNew = FALSE;
	}
    }
    else tableRunner(InfoShow, TRUE);
    PagingOff();
}

/*
 * InfoShow()
 *
 * This function will actually show the information for a room.
 */
static int InfoShow(int r)
{
    char *c;
    int  rover;
    extern int DirAlign;
    extern char AlignChar;

    if (outFlag != OUTOK) return 0;
    if ((c = GetInfo(roomTab[r].rtname)) != NULL) CleanEnd(c);
    if (c != NULL && strlen(c) != 0) {
	mPrintf("%s ", roomTab[r].rtname);
	for (rover = strlen(roomTab[r].rtname); rover < 22; rover++)
	    oChar('.');
	oChar(' ');
	DirAlign = 22;
	AlignChar = ' ';
	mPrintf("%s", c);
	DirAlign = 0;
	doCR();
    }
    if (c != NULL) free(c);
    return 0;
}

