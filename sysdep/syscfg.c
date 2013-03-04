/*
 *				syscfg.c
 *
 * Part of the configuration program for Citadel bulletin board system.  This
 * file contains the system dependent code!
 */

#define SYSTEM_DEPENDENT
#define CONFIGURE

#include "ctdl.h"
#include "sys\stat.h"
#include "c86door.h"
#include "conio.h"
#include "stdarg.h"
#include "psys.h"

/*
 *				History
 *
 * 89Jan15 HAW  Major release update: colors, etc....
 * 87Jan19 HAW  Created.
 */

/*
 *				Contents
 *
 * #    dirExists()		check to see if directory exists
 * #    doCommon()		handles common stuff
 *      GetDoorData()		reads in some piece of the door data
 *      initSysSpec()		initialization for system dependencies
 *      NextNonWhite()		finds next piece of non-whitespace
 *      NextWhite()		finds next piece of white space
 *      StoreDoor()		manages the reading a door
 *      SysDepIntegrity()       makes necessary checks for integrity
 *      sysSpecs()		System-dependent code for configure
 *
 *		# means local for this implementation
 */

static char curDisk;
static char curDir[80];
static char ourHomeSpace[100];
static char IBMcom = -1;
static SListBase DoorList  = { NULL, DoorName, NULL, free, NULL };
static int  TiCom = -1, isTI = FALSE;

	/***** THESE ARE REQUIRED DEFINITIONS! ******/
char *R_W_ANY    = "r+b";
char *W_R_ANY    = "w+b";
char *READ_ANY   = "rb";
char *WRITE_ANY  = "wb";
char *READ_TEXT  = "rt";
char *WRITE_TEXT = "wt";
char *A_C_TEXT    = "a+";
char IRQforce = FALSE;

	/*****  done  *****/

extern CONFIG cfg;       /* The configuration variable	*/
extern MessageBuffer   msgBuf;
extern rTable    *roomTab;	      /* RAM index of rooms		*/

#define LCR_OFFSET	0x0b
#define MD_OFFSET	0x08
#define LSTAT_OFFSET	0x0d
#define MSTAT_OFFSET	0x0e
#define MCR_OFFSET	0x0c
#define IER_OFFSET	0x09

struct {
	int Base;
	int PICMask;
	int Vector;
}  ComTable[] = {
	{ 0x3f0, 0xef, 12 },
	{ 0x2f0, 0xf7, 11 },
	{ 0x3e0, 0xef, 12 },
	{ 0x2e0, 0xf7, 11 },
};

/*
 * dirExists()
 *
 * This will check to see if the directory exists.
 */
void dirExists(char disk, char *theDir)
{
    struct stat buff;
    extern char FirstInit, ReInit;

    sprintf(msgBuf.mbtext, "%c:%s", disk + 'A', theDir);
    if (strlen(theDir) == 0) return ;
    if (msgBuf.mbtext[strlen(msgBuf.mbtext)-1] == '\\')
	msgBuf.mbtext[strlen(msgBuf.mbtext)-1] = '\0';
    if (stat(msgBuf.mbtext, &buff) == 0) {
	if (!(buff.st_mode & S_IFDIR)) {
	    sprintf(msgBuf.mbtext, "%c:%s is not a directory!",
					disk + 'A', theDir);
	    illegal(msgBuf.mbtext);
	}
    }
    else {
	printf("\nThe directory '%s' doesn't exist.  Create it? ", 
							msgBuf.mbtext);
	if (FirstInit)
	    printf("Y\n ");

	if (ReInit || FirstInit || toUpper(simpleGetch()) == 'Y')
	    if (mkdir(msgBuf.mbtext) == BAD_DIR)
		illegal("Couldn't make directory!");
    }
}

/*
 * sysSpecs()
 *
 * This is the main manager for the system specific configure stuff.
 */
int sysSpecs(char *line, int offset, char *status, FILE *fd)
{
    char var[90], string[90], work[150];
    int  arg;

    *status = TRUE;
    if (sscanf(line, "%s %s ", var, string)) {
	if (string[0] != '\"') {
	    arg = atoi(string);
	}
	if (strcmp(var, PIBM)    == SAMESTRING) {
	    cfg.DepData.IBM = arg;
	    if (cfg.DepData.IBM == 0 && TiCom != -1) {	/* TI Port handling */
		if (TiCom < 0 || TiCom > 3)
		    illegal("TI com port designation must be between 1 and 4.");
		cfg.DepData.TiComPort = TiCom;
	    }
	} else if (strcmp(var, OFFLINE_DELAY)    == SAMESTRING) {
	    cfg.DepData.InterCharDelay = arg;
	} else if (strcmp(var, COMPORT)    == SAMESTRING) {
		/*
		 * This code should be placed in a table someday.
		 * Unless we ever get a real interrupt driven package.
		 */
	    TiCom = arg - 1;
	    if (cfg.DepData.IBM == 0) {	/* TI Port handling */
		if (arg < 1 || arg > 4)
		    illegal("TI com port designation must be between 1 and 4.");
		cfg.DepData.TiComPort = arg - 1;
	    }
	    else if (arg == 1) {
		IBMcom = 1;
	    }
	    else if (arg == 2) {
		IBMcom = 2;
	    }
	    else if (arg == 3) {
		IBMcom = 3;
	    }
	    else if (arg == 4) {
		IBMcom = 4;
	    }
	    else illegal("COM port can only currently be 1 - 4");
	} else if (strcmp(var, "#TI"  )    == SAMESTRING) {
	    isTI = TRUE;
	} else if (strcmp(var, "#TI-Internal"  )    == SAMESTRING) {
	    cfg.DepData.TiInternal = TRUE;
	} else if (strcmp(var, "#door"  )    == SAMESTRING) {
	    StoreDoor(line, fd);
	} else if (strcmp(var, BANNER_COUNT) == SAMESTRING) {
	    cfg.DepData.BannerCount = arg;
	} else if (strcmp(var, NETRECEPT)   == SAMESTRING) {
	    anyArea(var, line, &cfg.receptArea.naDisk, cfg.receptArea.naDirname);
		sprintf(work, "%c:%s", 'A' + cfg.receptArea.naDisk,
					cfg.receptArea.naDirname);
		if (access(work, 0) != 0) {
			if (mkdir(work) != 0) {
				printf("WARNING: Could not create the Net Reception Directory (%s)\n", work);
			}
		}
	} else if (strcmp(var, OEDITOR) == SAMESTRING) {
	    cfg.BoolFlags.SysopEditor = TRUE;
	    readString(line, cfg.DepData.Editor, FALSE);
	} else if (strcmp(var, EDITAREA) == SAMESTRING) {
	    readString(line, cfg.DepData.EditArea, FALSE);
	} else if (strcmp(var, REINIT) == SAMESTRING) {
	    readString(line, cfg.DepData.HiSpeedInit, TRUE);
	    if (strlen(cfg.DepData.HiSpeedInit) >=
					(sizeof cfg.DepData.HiSpeedInit - 1))
		illegal("The #REINIT parameter is too long.  29 is the max.");
	    else if (cfg.DepData.HiSpeedInit[strlen(cfg.DepData.HiSpeedInit)]
				!= '\r')
		strcat(cfg.DepData.HiSpeedInit, "\r");
	} else if (strcmp(var, ENABLE) == SAMESTRING) {
	    readString(line, cfg.DepData.sEnable, TRUE);
	    if (strlen(cfg.DepData.sEnable) >= sizeof cfg.DepData.sEnable)
		illegal("The #ENABLE-MODEM parameter value is too long.");
	} else if (strcmp(var, DISABLE) == SAMESTRING) {
	    readString(line, cfg.DepData.sDisable, TRUE);
	    if (strlen(cfg.DepData.sDisable) >= sizeof cfg.DepData.sDisable)
		illegal("The #DISABLE-MODEM parameter value is too long.");
	} else if (strcmp(var, PCLOCK) == SAMESTRING) {
	    if (strCmpU(string, "none") == SAMESTRING)
		cfg.DepData.Clock = NO_CLOCK;
	    else if (strCmpU(string, "inuse") == SAMESTRING)
		cfg.DepData.Clock = BUSY_CLOCK;
	    else if (strCmpU(string, "always") == SAMESTRING)
		cfg.DepData.Clock = ALWAYS_CLOCK;
	    else illegal("Didn't understand #CLOCK value.");
	} else if (strcmp(var, FGROUND) == SAMESTRING) {
	    cfg.DepData.ScreenColors.ScrFore = ScreenColor(line);
	} else if (strcmp(var, BGROUND) == SAMESTRING) {
	    cfg.DepData.ScreenColors.ScrBack = ScreenColor(line);
	} else if (strcmp(var, SFGROUND) == SAMESTRING) {
	    cfg.DepData.ScreenColors.StatFore = ScreenColor(line);
	} else if (strcmp(var, SBGROUND) == SAMESTRING) {
	    cfg.DepData.ScreenColors.StatBack = ScreenColor(line);
	} else if (strcmp(var, OLDVIDEO) == SAMESTRING) {
	    cfg.DepData.OldVideo = TRUE;
	} else if (strcmp(var, LOCKPORT) == SAMESTRING) {
	    if (arg < 0 || arg > 8)
		illegal("Bad value for #LOCK-PORT: 0-6 valid only.");
	    cfg.DepData.LockPort = arg;
	} else if (strcmp(var, MDMSETUP) == SAMESTRING) {
	    readString(line, cfg.codeBuf + offset, TRUE);
	    strcat(cfg.codeBuf + offset, "\r");
	    cfg.DepData.pInitString = offset;
	    do
		offset++;
	    while (cfg.codeBuf[offset]);
	    offset++;
	}
	else *status = FALSE;
    }
    return offset;
}

#define makeN(x)	x + cfg.codeBuf

/*
 * anyArea()
 *
 * This does common work on the area stuff.
 */
void anyArea(char *var, char *line, char *disk, char *target)
{
    readString(line, target, FALSE);

    MSDOSparse(target, disk);
}

/*
 * initSysSpec()
 *
 * This is initialization for system dependencies.
 */
void initSysSpec()
{
    cfg.DepData.pInitString = 0;
    cfg.DepData.InterCharDelay = 0;
    cfg.DepData.IBM = -1;
    cfg.DepData.LockPort = -1;
    cfg.DepData.OldVideo = FALSE;
    getcwd(curDir, 79);
    strcpy(ourHomeSpace, curDir);
    curDisk = curDir[0] - 'A';
    cfg.DepData.Editor[0] = 0;
    cfg.DepData.EditArea[0] = 0;
    cfg.DepData.HiSpeedInit[0] = 0;
    cfg.DepData.Clock = ALWAYS_CLOCK;
    cfg.DepData.sEnable[0] = 0;
    cfg.DepData.sDisable[0] = 0;
    cfg.DepData.TiInternal = FALSE;
    cfg.DepData.TiComPort = 0;
    cfg.DepData.BannerCount = 1;
    zero_array(cfg.DepData.DialPrefixes);
}

/*
 * MSDOSparse()
 *
 * This parses a string in the standard format.
 */
void MSDOSparse(char *theDir, char *drive)
{
    if (theDir[1] == ':') {
	*drive = toUpper(theDir[0]) - 'A';
	strcpy(theDir, theDir+2);
    }
    else {
	*drive = curDisk;
    }
}

#define WhiteSpace(x)   ((x) == ' ' || (x) == '\t' || (x) == '\n')
/*
 * #door <entrycode> <location> <who> <where> <how long>
 * <description>
 * <command line>
 */

/*
 * StoreDoor()
 *
 * This manages the reading and interpretation of a door.
 */
void StoreDoor(char *line, FILE *fd)
{
    char     *s;
    DoorData *DoorInfo;
    char     l2[100];
    char     temp[15];
    int      i, j;

    DoorInfo = (DoorData *) GetDynamic(sizeof *DoorInfo);
    zero_struct(*DoorInfo);

    line += 5;  /* now we point at the space before the entry code. */

    /* get entrycode */
    if ((line = GetDoorData(line, DoorInfo->entrycode, 6)) == NULL)
	illegal("Problem with parsing the entry code for a door!");

    /* get door preferred location */
    if ((line = GetDoorData(line, DoorInfo->location, 50)) == NULL)
	illegal("Problem with parsing the location of a door! (d1)");

    /* get door privilege */
    if ((line = GetDoorData(line, temp, 15)) == NULL)
	illegal("Problem with parsing the location of a door (d2)!");

    if (strCmpU(temp, "anyone") == SAMESTRING)
	DoorInfo->flags |= DOOR_ANYONE;
    else if (strCmpU(temp, "aide") == SAMESTRING)
	DoorInfo->flags |= DOOR_AIDE;
    else if (strCmpU(temp, "sysop") == SAMESTRING)
	DoorInfo->flags |= DOOR_SYSOP;
    else if (strCmpU(temp, "autodoor") == SAMESTRING)
	DoorInfo->flags |= DOOR_AUTO;
    else if (strCmpU(temp, "newusers") == SAMESTRING)
	DoorInfo->flags |= DOOR_NEWUSER;
    else if (strCmpU(temp, "onlogin") == SAMESTRING)
	DoorInfo->flags |= DOOR_ONLOGIN;
    else illegal("Could not identify who is allowed to use this door!");

    /* get MODEM/CONSOLE/EITHER flag */
    if ((line = GetDoorData(line, temp, 15)) == NULL)
	illegal("Problem with parsing the 'where' field!");

    if (strCmpU(temp, "anywhere") == SAMESTRING) {
	DoorInfo->flags |= DOOR_CON;
	DoorInfo->flags |= DOOR_MODEM;
    }
    else if (strCmpU(temp, "console") == SAMESTRING)
	DoorInfo->flags |= DOOR_CON;
    else if (strCmpU(temp, "modem") == SAMESTRING)
	DoorInfo->flags |= DOOR_MODEM;
    else illegal("Could not identify the 'where' value!");

    /* get time limit of door */
    if ((line = GetDoorData(line, temp, 15)) == NULL)
	illegal("Problem with parsing the 'time' field!");

    if (strCmpU(temp, "unlimited") == SAMESTRING)
	DoorInfo->TimeLimit = -1;
    else
	DoorInfo->TimeLimit = atoi(temp);

    /* Now get optional room to tie this door to */
    if ((s = strrchr(line, '\n')) != NULL) *s = 0;
    strcpy(DoorInfo->RoomName, line);

    /* get description of door */
    if (fgets(l2, 100, fd) == NULL)
	illegal("Unexpected EOF in door handling!");

    if ((s = strrchr(l2, '\n')) != NULL) *s = 0;

    if (strlen(l2) >= 80)
	illegal("Description too long!");

    strcpy(DoorInfo->description, l2);

    /* get parameters of door */
    if (fgets(l2, 100, fd) == NULL)
	illegal("Unexpected EOF in door handling!");

    if ((s = strrchr(l2, '\n')) != NULL) *s = 0;

    /* Do a simple minded parse of the doors parameters */
    if (!(DoorInfo->flags & (DOOR_NEWUSER|DOOR_ONLOGIN))) {
	for (i = j = 0; l2[i]; )
	    if (l2[i] == '%') {		/* Signals variable substitution */
		if (l2[++i] == '%') {       /* Really wants a '%' here!      */
		    DoorInfo->CommandLine[j++] = '%';
		    i++;
		}
		else	/* simply calculate the needed value.   */
		    DoorInfo->CommandLine[j++] = l2[i++] - 'a' + 1;
	    }
	    else DoorInfo->CommandLine[j++] = l2[i++];

	DoorInfo->CommandLine[j] = 0;	/* And seal up end of string */
    }
    else strcpy(DoorInfo->CommandLine, l2);

    AddData(&DoorList, DoorInfo, NULL, FALSE);
}

/*
 * GetDoorData()
 *
 * This reads in some piece of the door data.
 */
char *GetDoorData(char *line, char *field, int size)
{
    char *s;

    if ((line = NextNonWhite(line)) == NULL)
	return NULL;

	/* Now skip over field so we may zero out & copy */
    if ((s = NextWhite(line)) == NULL)
	return NULL;

    *s = 0;	/* zero whitespace */

    if (strlen(line) >= size) {
	printf("Value '%s' too long for door's data field.\n", line);
	return NULL;
    }

    strcpy(field, line);

    return s + 1;
}

/*
 * NextWhite()
 *
 * This finds the next piece of white space on the line.
 */
char *NextWhite(char *line)
{
    char *s;

    for (s = line; !WhiteSpace(*s) && *s; s++)
	;

    if (!(*s)) return NULL;

    return s;
}

/*
 * NextNonWhite()
 *
 * This finds next piece of non-whitespace.
 */
char *NextNonWhite(char *line)
{
    char *s;

    for (s = line; WhiteSpace(*s) && *s; s++)
	;

    if (!(*s)) return NULL;

    return s;
}

FILE *upfd;

/*
 * WriteDoors()
 *
 * This writes out the list of doors.
 */
void WriteDoors()
{
    SYS_FILE DoorFile;
    FILE *doorfl;

    makeSysName(DoorFile, DOOR_DATA, &cfg.roomArea);
    if ((doorfl = fopen(DoorFile, WRITE_ANY)) == NULL) {
	sprintf(msgBuf.mbtext, "Could not create %s.", DoorFile);
	illegal(msgBuf.mbtext);
    }
    RunListA(&DoorList, WrtDoor, (void *) doorfl);
    fclose(doorfl);
}

/*
 * WrtDoor()
 *
 * And this writes out a single door.
 */
void WrtDoor(DoorData *d, FILE *doorfl)
{
    fwrite(d, sizeof *d, 1, doorfl);
}

/*
 * findcolor()
 *
 * This will interpret a string to a value.
 */
int findcolor(char *str)
{
    int i;
    static struct {
	char *name;
	int  code;
    } colors[] = {
	{ "BLACK\n", BLACK },
	{ "WHITE\n", WHITE },
	{ "BLUE\n",  BLUE  },
	{ "GREEN\n", GREEN },
	{ "CYAN\n",  CYAN  },
	{ "RED\n",   RED   },
	{ "MAGENTA\n", MAGENTA },
	{ "BROWN\n",   BROWN },
	{ "LIGHT GRAY\n", LIGHTGRAY },
	{ "DARK GRAY\n", DARKGRAY },
	{ "LIGHT BLUE\n", LIGHTBLUE },
	{ "LIGHT GREEN\n", LIGHTGREEN },
	{ "LIGHT CYAN\n", LIGHTCYAN },
	{ "LIGHT RED\n", LIGHTRED },
	{ "LIGHT MAGENTA\n", LIGHTMAGENTA },
	{ "YELLOW\n", YELLOW}
    };

    for (i = 0; i < 16; i++)
	if (strCmpU(str, colors[i].name) == 0)
	    return colors[i].code;

    return BLACK;
}

/*
 * ScreenColor()
 *
 * And this handles parsing.
 */
int ScreenColor(char *line)
{
    char *s;

    if ((s = NextWhite(line)) == NULL)
	illegal("Couldn't parse the color.");

    if ((s = NextNonWhite(s)) == NULL)
	illegal("Couldn't parse the color.");

    return findcolor(s);
}

/*
 * SysDepIntegrity()
 *
 * Ths makes necessary checks for integrity.  It calls illegal() if there's
 * a failure.
 */
char SysDepIntegrity(int *offset)
{
    extern int  DefaultPrefix;
    char bad = FALSE;
    int i;

    if (cfg.DepData.IBM == -1) {
	printf("You didn't specify if this is IBM or Zenith!\n");
	bad = TRUE;
    }

    if (cfg.DepData.IBM) {
	if (IBMcom == -1) {
	    printf("Need a COM setting for PClones!\n");
	    bad = TRUE;
	}
	else {
	    cfg.DepData.modem_data = ComTable[IBMcom - 1].Base + MD_OFFSET;
	    cfg.DepData.modem_status = ComTable[IBMcom - 1].Base + MSTAT_OFFSET;
	    cfg.DepData.line_status = ComTable[IBMcom - 1].Base + LSTAT_OFFSET;
	    cfg.DepData.mdm_ctrl = ComTable[IBMcom - 1].Base + MCR_OFFSET;
	    cfg.DepData.ln_ctrl = ComTable[IBMcom - 1].Base + LCR_OFFSET;
	    cfg.DepData.ier = ComTable[IBMcom - 1].Base + IER_OFFSET;
	    cfg.DepData.com_vector = ComTable[IBMcom - 1].Vector;
	    if (!IRQforce) cfg.DepData.PIC_mask = ComTable[IBMcom - 1].PICMask;
	}
    }

    if (bad)
	illegal("See above.");

    WriteDoors();

/* kludge for 3.43 - kill on next major release, back to portable code */
    for (i = 0; i <= B_9; i++)
	if (cfg.DepData.DialPrefixes[i] == 0) {
	    cfg.DepData.DialPrefixes[i] = DefaultPrefix;
	}

    return TRUE;
}

void *EatDirEntry(char *line);
char DirDeleted = FALSE;
SListBase DirRooms = { NULL, NULL, NULL, NULL, EatDirEntry };
/*
 * FinalSystemCheck()
 *
 * This does the final system dependent checks.
 */
int FinalSystemCheck(char OnlyParams)
{
    FILE *Dirs;
    SYS_FILE fn;
    void WriteDir();

    /* Check for excess entries in ctdldir.sys */
    makeSysName(fn, "ctdldir.sys", &cfg.roomArea);
    MakeList(&DirRooms, fn, NULL);
    if (DirDeleted) {
	if ((Dirs = fopen(fn, WRITE_TEXT)) != NULL) { 
	    RunListA(&DirRooms, WriteDir, (void *) Dirs);
	    fclose(Dirs);
	}
    }
    return TRUE;
}

void WriteDir(char *l, FILE *Dirs)
{
    fprintf(Dirs, "%s\n", l);
}

/*
 * EatDirEntry()
 *
 * This validates an entry from ctdldir.sys.
 */
void *EatDirEntry(char *line)
{
    int  room;

    /* discard garbled entries */
    if (strchr(line, ' ') == NULL) {
	DirDeleted = TRUE;
	printf("Garbled entry -%s- deleted from ctdldir.sys.\n", line);
	return NULL;
    }

    room = atoi(line);

    if (room >= MAXROOMS || room <= AIDEROOM) {
	DirDeleted = TRUE;
	printf("Out of range entry -%s- deleted from ctdldir.sys.\n", line);
	return NULL;
    }

    if (roomTab[room].rtflags.INUSE && roomTab[room].rtflags.ISDIR)
	return strdup(line);

    DirDeleted = TRUE;
    printf("Excess entry -%s- deleted from ctdldir.sys.\n", line);
    return NULL;
}

int Dcount;

/*
 * FindDoorSlot()
 *
 * This looks for the specified door for autodoor stuff.
 */
int FindDoorSlot(char *name)
{
    int *c;
    DoorData temp;

    Dcount = 0;
    strcpy(temp.entrycode, name);
    if ((c = (int *) SearchList(&DoorList, &temp)) != NULL)
	return *c;
    return 0;
}

/*
 * DoorName()
 *
 * This will check to see if this door is here.  Used for autodoor stuff.
 */
void *DoorName(DoorData *d, DoorData *s)
{
    if (strCmpU(d->entrycode, s->entrycode) == SAMESTRING)
	return &Dcount;

    Dcount++;
    return NULL;
}

/*
 * mPrintf()
 *
 * formats format+args to console.
 */
int mPrintf(char *format, ...)
{
    va_list argptr;
    char    garp[2000];

    va_start(argptr, format);
    vsprintf(garp, format, argptr);
    va_end(argptr);
    printf("%s", garp);
    return 0;
}

/*
 * sysArgs()
 *
 * Handles a command line argument.  Return FALSE if not recognized, TRUE
 * otherwise.
 */
char sysArgs(char *str)
{
    int val;

    if (strncmp(str, "irq=", 4) == SAMESTRING) {
	IRQforce = TRUE;
	val = atoi(str + 4);
	cfg.DepData.PIC_mask = 0xff ^ (1 << val);
printf("PIC mask of %x generated.\n", cfg.DepData.PIC_mask);
	return TRUE;
    }
    return FALSE;
}

int doArea(char *var, char *line, SYS_AREA *area, int offset)
{
    int old;

    anyArea(var, line, &area->saDisk, cfg.codeBuf + offset);
    area->saDirname = old = offset;
    while (cfg.codeBuf[offset]) /* step over string     */
	offset++;

    if (strchr(cfg.codeBuf + old, '\\') != NULL)
	illegal("Directory name cannot have a '\\' in it!");

    if (old != offset) {
	cfg.codeBuf[offset++] = '\\';
	cfg.codeBuf[offset] = 0;
    }
    offset++;
    return offset;
}

/*
 * AreaCheck()
 *
 * Check the specified area.
 */
int AreaCheck(SYS_AREA *area)
{
    dirExists(area->saDisk, makeN(area->saDirname));
    return 0;
}

FILE *safeopen(char *fn, char *mode)
{
    struct stat buff;

    if (stat(fn, &buff) == 0)
	if (buff.st_mode & S_IFCHR)
	    return NULL;

    return fopen(fn, mode);
}

/*
 * ReadDialOut()
 *
 * This function reads in a dial out prefix.
 */
int ReadDialOut(char *line, int baud, int *offset)
{
    readString(line, &cfg.codeBuf[*offset], TRUE);
    cfg.DepData.DialPrefixes[baud] = *offset;
    while (cfg.codeBuf[*offset])
	(*offset)++;
    (*offset)++;
    return TRUE;
}

/*
 * GetDir()
 *
 * This function extracts a useable string from the directory hidden in it.
 * This is used for symbolic names in redirection events.
 */
char *GetDir(SYS_AREA *area)
{
    static char name[20];

    sprintf(name, "%c:%s", area->saDisk + 'a',
                                  cfg.codeBuf + area->saDirname);

    if (name[strlen(name) - 1] == '\\')
	name[strlen(name) - 1] = 0;

    return name;
}

/*
 * GetDomainDirs()
 *
 * Get the list of domain directories.
 */
void GetDomainDirs(SListBase *list)
{
	struct ffblk f;
	char done;
	char fn[50];

	sprintf(fn, "%c:%s*.*", cfg.domainArea.saDisk + 'a',
				cfg.codeBuf + cfg.domainArea.saDirname);
	done = findfirst(fn, &f, FA_DIREC);
	while (!done) {
		if ((f.ff_attrib & FA_DIREC) &&
			strcmp(f.ff_name, ".") && strcmp(f.ff_name, "..")) {
			AddData(list, strdup(f.ff_name), NULL, FALSE);
		}
		done = findnext(&f);
	}
}

/*
 * SysWildCard()
 *
 * This should get a list of files from the current "area".
 * Returns the # of files listed that fit the given mask.
 */
void SysWildCard(void (*fn)(DirEntry *amb), char *mask)
{
	struct ffblk   FlBlock;
	extern char    *monthTab[13];
	DirEntry       fp;
	int		   done;

	for (done = findfirst(mask, &FlBlock, 0); !done; 
						done = findnext(&FlBlock)) {
		fp.unambig = strdup(FlBlock.ff_name);
		fp.FileSize = FlBlock.ff_fsize;
		(*fn)(&fp);
		free(fp.unambig);
	}
}

#define SETDISK		14      /* MSDOS change default disk function   */
/*
 * homeSpace()
 *
 * takes us home!
 */
void homeSpace()
{
    if (strLen(curDir) != 0) {
	chdir(curDir);
	curDir[0] = 0;
    }
    DoBdos(SETDISK, curDisk - 'A');
    chdir(ourHomeSpace);
}

/*
 * realSetSpace()
 *
 * This does the real work of SetSpace.
 */
char realSetSpace(char disk, char *dir)
{
    DoBdos(SETDISK, disk);

    getcwd(curDir, 99);

    if (strLen(dir) != 0 && chdir(dir) == BAD_DIR) {
	homeSpace();
	return FALSE;
    }

    return TRUE;
}

void MoveToSysDirectory(SYS_AREA *area)
{
    SYS_FILE fn;

    sprintf(fn, "%c:%s", area->saDisk + 'a', cfg.codeBuf + area->saDirname);
    fn[strlen(fn) - 1] = 0;
    SetSpace(fn);
}

/*
 * SetSpace()
 *
 * This moves us to an area associated with the specified room.
 */
char SetSpace(char *area)
{
    char   dir[150], drive;

    if (area == NULL) {
	mPrintf("?Directory not present! (internal error)\n ");
	return FALSE;
    }
    strCpy(dir, area);
    MSDOSparse(dir, &drive);
    if (!realSetSpace(toUpper(drive) - 'A', dir)) {
	mPrintf("?Directory not present!\n ");
	return FALSE;
    }
    return TRUE;
}

/*
 * getRawDate()
 *
 * This function gets the raw date from MSDOS.
 */
void getRawDate(int *year, int *month, int *day, int *hours, int *minutes,
						int *seconds, int *milli)
{
    struct date dateblk;
    struct time timeblk;

    getdate(&dateblk);
    gettime(&timeblk);

    *year  = dateblk.da_year;
    *month = dateblk.da_mon;
    *day  = dateblk.da_day ;
    *hours = timeblk.ti_hour;
    *minutes = timeblk.ti_min ;
    *seconds = timeblk.ti_sec ;
    *milli = timeblk.ti_hund;
}
