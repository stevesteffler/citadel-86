/*
 *				confg2.c
 *
 * Event configuration program for Citadel bulletin board system.
 */

#define CONFIGURE

#include "ctdl.h"

#define SYMBOLICS

/*
 *				History
 *
 * 87Oct02 HAW  Created.
 */

/*
 *				Contents
 *
 *	checkList()		searches for a given string
 *	EatEvent()		Digests a #event
 *	FigureNets()		eats a comma separated list of nets
 *	getLVal()		gets a field value from a #event line
 */

int EvWCt    = 0;

#define PARA1 "\nWARNING: An event of Class dl-time should only use type quiet\n"
#define PARA2 "\nWARNING: An event of Class anytime-net should only use type quiet\n"
#define PARA3 "\nWARNING: Events of Class chat-on and chat-off should only use type quiet\n"
#define PARA4 "\nWARNING: Events of Class newusers-allowed and newusers-disallowed should only use type quiet\n"
#define PARA5 "Events of Class until-done-net cannot be of type quiet.\n"
#define PARA6 "Events of Class netcache cannot be of type quiet.\n"

extern rTable    *roomTab;

typedef struct {
    char *GenName;
    int  GenVal;
} GenList;

static GenList EvnDays[] = {
	{ "Sun", SUNDAYS },
	{ "Mon", MONDAYS },
	{ "Tue", TUESDAYS },
	{ "Wed", WEDNESDAYS },
	{ "Thu", THURSDAYS },
	{ "Fri", FRIDAYS },
	{ "Sat", SATURDAYS },
	{ "All", ALL_DAYS }
} ;

static GenList EvnTypes[] = {
	{ "preempt", TYPREEMPT },
	{ "non-preempt", TYNON },
	{ "quiet", TYQUIET }
} ;

static GenList EvCls[] = {
	{ "network", CLNET },
	{ "external", CLEXTERN },
	{ "dl-time", CL_DL_TIME },
	{ "anytime-net", CL_ANYTIME_NET },
	{ "door-limit", CL_DOOR_TIME },
	{ "autodoor", CL_AUTODOOR },
	{ "chat-on", CL_CHAT_ON },
	{ "chat-off", CL_CHAT_OFF },
	{ "redirect", CL_REDIRECT },
	{ "newusers-allowed", CL_NEWUSERS_ALLOWED },
	{ "newusers-disallowed", CL_NEWUSERS_DISALLOWED },
	{ "until-done-net", CL_UNTIL_NET },
	{ "netcache", CL_NETCACHE },
} ;

SListBase Events = { NULL, NULL, NULL, EvFree, NULL };
/*
 * Some things cannot be done immediately ..
 */
SListBase ProcessLater = { NULL, NULL, NULL, NULL, NULL };

extern CONFIG cfg;			/* The configuration variable   */
extern EVENT  *EventTab;

typedef struct {
    EVENT *Evt;
    char  doorname[10];			/* inefficient?  who cares...   */
} DLK;

typedef struct {
    EVENT *Evt;
    char *line;
    SListBase list;
} Redirect;

int checkList(char *ptr, GenList listing[], int elements);
/*
 * EatEvent()
 *
 * This function assimilates event parameters.  Format:
 *
 *    #event <days> <time> <class> <type> <duration> <warning string> <dep>
 */
int EatEvent(char *line, int offset)
{
    EVENT  *EvBuf;
    DLK    *DataLk;
    Redirect *red;
    char   *ptr, temp[10];
    int    rover, i, WeekDays;
    int    EvHour, EvMin;
    UNS_32 Days = 0, ii;
    char   pl = FALSE;

    EvBuf = (EVENT *) GetDynamic(sizeof *EvBuf);

    if ((ptr = strchr(line, '\n')) != NULL)
	*ptr = 0;

    rover		= 6;	/* starting index into line '#event ' ... */
    WeekDays		= FigureDays(getLVal(line, &rover, ' '), &Days);
    EvHour		= atoi(getLVal(line, &rover, ':'));
    EvMin		= atoi(getLVal(line, &rover, ' '));
    EvBuf->EvTimeDay = EvBuf->EvMinutes	= EvHour * 60 + EvMin;
    ptr			= getLVal(line, &rover, ' ');
    EvBuf->EvClass	= checkList(ptr, EvCls, NumElems(EvCls));
    if (EvHour > 23 || EvMin > 59)
	illegal("Bad time specified for an event!\n");
    EvBuf->EvMinutes    = EvHour * 60 + EvMin;
    ptr			= getLVal(line, &rover, ' ');
    EvBuf->EvType	= checkList(ptr, EvnTypes, NumElems(EvnTypes));
    EvBuf->EvDur	= atoi(getLVal(line, &rover, ' '));
    EvBuf->EvWarn	= GetStoreQuote(line, cfg.codeBuf, &rover, &offset);
    switch (EvBuf->EvClass) {
	case CL_ANYTIME_NET:
	    EvBuf->vars.Anytime.EvDeadTime = atoi(getLVal(line, &rover, ' ')) * 60l;
	    EvBuf->vars.Anytime.EvAnyDur   = atoi(getLVal(line, &rover, ' '));
	    /* no break */
	case CLNET:
	case CL_UNTIL_NET:
	    EvBuf->EvExitVal = FigureNets(getLVal(line, &rover, ' '));
	    if (EvBuf->EvClass == CL_ANYTIME_NET && EvBuf->EvType != TYQUIET) {
		printf(PARA2);
		EvBuf->EvType = TYQUIET;
	    }
	    if (EvBuf->EvClass == CL_UNTIL_NET && EvBuf->EvType == TYQUIET) {
		illegal(PARA5);
	    }
	    break;
	case CL_NETCACHE:
	    if (EvBuf->EvType == TYQUIET) {
		illegal(PARA6);
	    }
	    EvBuf->EvExitVal = FigureNets(getLVal(line, &rover, ' '));
	    break;
	case CL_AUTODOOR:
	    i = 0;
	    GetStoreQuote(line, EvBuf->vars.EvUserName, &rover, &i);
	    strcpy(temp, getLVal(line, &rover, ' '));
	    break;
	case CL_CHAT_ON:
	case CL_CHAT_OFF:
	    if (EvBuf->EvType != TYQUIET) {
		printf(PARA3);
		EvBuf->EvType = TYQUIET;
	    }
	    break;
	case CL_NEWUSERS_ALLOWED:
	case CL_NEWUSERS_DISALLOWED:
	    if (EvBuf->EvType != TYQUIET) {
		printf(PARA4);
		EvBuf->EvType = TYQUIET;
	    }
	    break;
	case CL_REDIRECT:	/* filename targetdir systemname */
#ifndef SYMBOLICS
	    strcpy(EvBuf->vars.Redirect.EvFilename, getLVal(line, &rover, ' '));
	    EvBuf->vars.Redirect.EvHomeDir = offset;
	    strcpy(cfg.codeBuf + offset, getLVal(line, &rover, ' '));
	    while (cfg.codeBuf[offset++])
		;
	    strcpy(EvBuf->vars.Redirect.EvSystem, line + rover + 1);
#else
	    red = GetDynamic(sizeof *red);
	    red->Evt = EvBuf;
	    red->line = strdup(line + rover);
	    InitListValues(&red->list, NULL, NULL, NULL, NULL);
	    AddData(&ProcessLater, red, NULL, FALSE);
	    pl = TRUE;
#endif
	    break;
	case CLEXTERN:
	case CL_DL_TIME:
	case CL_DOOR_TIME:
	    EvBuf->EvExitVal = (MULTI_NET_DATA) atoi(getLVal(line, &rover, ' '));
	    if (EvBuf->EvType != TYQUIET && EvBuf->EvClass == CL_DL_TIME) {
		printf(PARA1);
		EvBuf->EvType = TYQUIET;
	    }
	    if (EvBuf->EvClass != CL_DL_TIME &&
		EvBuf->EvClass != CL_DOOR_TIME) {
		if (EvBuf->EvExitVal >=0 && EvBuf->EvExitVal < 5)
		    printf("\n\007WARNING: Event ERRORLEVEL value is "
			   "between 0 and 4, all of which are used by "
			   "Citadel.\007\n");
	    }
	    break;
    }

    /* now check to see which days this event is active */
    for (rover = 1, ii = 1; rover < 32; rover++) {
	if (Days & ii) {
	    DataLk = (DLK *) GetDynamic(sizeof *DataLk);
	    DataLk->Evt = (EVENT *) GetDynamic(sizeof *EvBuf);
	    memcpy(DataLk->Evt, EvBuf, sizeof *EvBuf);
	    strcpy(DataLk->doorname, temp);
	    DataLk->Evt->EvFlags |= EV_DAY_BASE;
	    DataLk->Evt->EvDay = rover;
	    AddData(&Events, DataLk, NULL, FALSE);
	    cfg.EvNumber++;
	    if (pl) AddData(&red->list, DataLk->Evt, NULL, FALSE);
	}
	ii = ii << 1;
    }

    for (rover = 0, i = 1; rover < 7; EvBuf->EvMinutes += 1440, rover++) {
	if (WeekDays & i) {
	    DataLk = (DLK *) GetDynamic(sizeof *DataLk);
	    DataLk->Evt = (EVENT *) GetDynamic(sizeof *EvBuf);
	    memcpy(DataLk->Evt, EvBuf, sizeof *EvBuf);
	    strcpy(DataLk->doorname, temp);
	    AddData(&Events, DataLk, NULL, FALSE);
	    cfg.EvNumber++;
	    if (pl) AddData(&red->list, DataLk->Evt, NULL, FALSE);
	}
	i = i << 1;
    }

    return offset;
}

#ifdef SYMBOLICS
/*
 * RedirectProcess()
 *
 * This function is used to handle post processing of redirect events, which
 * may be using symbolic names that are not yet defined.
 */
void RedirectProcess(Redirect *red, int *offset)
{
    int SymbolicName(char *name, int *offset);
    int rover = 0;
    void EachEvent();

    strcpy(red->Evt->vars.Redirect.EvFilename, getLVal(red->line, &rover, ' '));
    red->Evt->vars.Redirect.EvHomeDir = SymbolicName(getLVal(red->line,
							&rover, ' '), offset);
    strcpy(red->Evt->vars.Redirect.EvSystem, red->line + rover + 1);
    RunListA(&red->list, EachEvent, red);
}

/*
 * EachEvent()
 *
 * Handle each real event associated with the meta event for a redirect
 * type of event.
 */
void EachEvent(EVENT *evt, Redirect *red)
{
    evt->vars.Redirect.EvHomeDir = red->Evt->vars.Redirect.EvHomeDir;
    strcpy(evt->vars.Redirect.EvFilename, red->Evt->vars.Redirect.EvFilename);
    strcpy(evt->vars.Redirect.EvSystem, red->Evt->vars.Redirect.EvSystem);
}

struct {
	char *name;
	SYS_AREA *area;
} Symbolics[] = {
	"#netarea", &cfg.netArea,
	"#homearea", &cfg.homeArea,
	"#msgarea", &cfg.msgArea,
	"#logarea", &cfg.logArea,
	"#roomarea", &cfg.roomArea,
	"#floorarea", &cfg.floorArea,
	"#domainarea", &cfg.domainArea,
	"#auditarea", &cfg.auditArea,
	"#holdarea", &cfg.holdArea,
	"#bioarea", &cfg.bioArea,
	"#msg2area", &cfg.msg2Area,
	"#infoarea", &cfg.infoArea,
};
/*
 * SymbolicName()
 *
 * This function handles possible symbolic name stuff.
 */
int SymbolicName(char *name, int *offset)
{
    int rover;

    if (*name == '#') {
	for (rover = 0; rover < NumElems(Symbolics); rover++)
	    if (strnicmp(name, Symbolics[rover].name,
			strlen(Symbolics[rover].name)) == 0) {
		break;
	    }
	if (rover == NumElems(Symbolics)) {
	    strcpy(cfg.codeBuf + *offset, name);
	}
	else {
	    strcpy(cfg.codeBuf + *offset, GetDir(Symbolics[rover].area));
	    strcat(cfg.codeBuf + *offset, name + strlen(Symbolics[rover].name));
	}
    }
    else
	strcpy(cfg.codeBuf + *offset, name);
    rover = *offset;
    while (cfg.codeBuf[*offset++])
        ;
    return rover;
}
#else
void RedirectProcess(Redirect *red, int *offset)
{
}
#endif

/*
 * checkList()
 *
 * This function searches for a given string in a list of arrays.  This is
 * used for both classes and types.
 */
int checkList(char *ptr, GenList listing[], int elements)
{
    int rover;
    char message[100];

    for (rover = 0; rover < elements; rover++)
	if (strCmpU(ptr, listing[rover].GenName) == SAMESTRING)
	    return listing[rover].GenVal;

    sprintf(message, "'%s' is not recognized!\n", ptr);
    illegal(message);
    return ERROR;
}

/*
 * getLVal()
 *
 * This function gets a field value from a #event line.
 */
char *getLVal(char *line, int *rover, char fin)
{
    static char retVal[75];
    int         i;

    if (!line[*rover]) {
	retVal[0] = 0;
	return retVal;
    }
    if (line[*rover] != '\n')
	(*rover)++;
    while (line[*rover] == ' ') (*rover)++;
    i = 0;
    while (line[*rover] != fin && line[*rover] != '\n' &&
				  line[*rover] != 0       ) {
	retVal[i++] = line[*rover];
	(*rover)++;
    }
    retVal[i] = 0;
    return retVal;
}

/*
 * FigureNets()
 *
 * This function takes a comma separated list of nets and eats it.
 */
MULTI_NET_DATA FigureNets(char *str)
{
    MULTI_NET_DATA retVal, r;
    int temp;

    retVal = 0l;
    while (*str) {
	temp = atoi(str);
	if (temp < 1 || temp > 31) illegal("Bad member net value (0 < x < 32");
	r = 1l;
	retVal = retVal + (r << (temp - 1));
	while (*str != ',' && *str) str++;
	if (*str) str++;
    }
    return retVal;
}

/*
 * GetStoreQuote()
 *
 * This function reads in a quoted string and stores it in codeBuf.
 */
static int GetStoreQuote(char *line, char *target, int *rover, int *offset)
{
    int OldOffset;

    OldOffset = *offset;
    while (line[*rover] == ' ') (*rover)++;
    if (line[*rover] != '\"')
	illegal("Expecting a quote mark in event processor!\n");
    (*rover)++;
    if (line[*rover] == '\"') {
	(*rover)++;
	return ERROR;
    }
    while (line[*rover] != '\"' && line[*rover] != '\r') {
	target[(*offset)++] = line[*rover];
	(*rover)++;
    }
    target[(*offset)++] = 0;
    (*rover)++;

    return OldOffset;
}

/*
 * FigureDays()
 *
 * This function reads and interprets the <days> field.
 */
int FigureDays(char *vals, UNS_32 *Days)
{
    int results, rover;
    char val[4];
    UNS_32 EatDay(char *v);

    results = 0;
    while (*vals) {
	for (rover = 0; rover < 3; rover++) {
	    val[rover] = *vals++;
	    val[rover+1] = 0;
	    if (*vals == ',' || !(*vals)) break;
	}
	if (isalpha(val[0]))
	    results += checkList(val, EvnDays, NumElems(EvnDays));
	else
	    *Days |= EatDay(val);

	if (*vals) vals++;      /* bypass ',' */
    }
    return results;
}

/*
 * EatDay()
 *
 * This function eats a day of the month specification and returns a
 * representative bitmask.
 */
static UNS_32 EatDay(char *v)
{
    int x;

    if ((x = atoi(v)) == 0) return (UNS_32) 0;
    return ((UNS_32) 1 << (x-1));
}

/*
 * EvIsDoor()
 *
 * This function is used by RunList, etc.  It does final init on #events
 * which control autodoors.
 */
void EvIsDoor(DLK *d)
{
    if (d->Evt->EvClass == CL_AUTODOOR)
	d->Evt->EvExitVal = (MULTI_NET_DATA) FindDoorSlot(d->doorname);
}

/*
 * EvFree()
 *
 * This function frees part of a list of events.  Superfluous????
 */
void EvFree(DLK *d)
{
    free(d->Evt);
    free(d);
}

/*
 * EventWrite()
 *
 * This function moves the element to the designated array position.
 */
void EventWrite(DLK *d)
{
    memcpy(EventTab + EvWCt++, d->Evt, sizeof (EVENT));
}

void *FindDir();
SListBase list = { NULL, FindDir, NULL, free, NULL };
/*
 * DomainIntegrity()
 *
 * This function is responsible for checking the integrity of MAP.SYS against
 * the actual data.
 */
void DomainIntegrity()
{
	extern SListBase DomainMap;
	int IntegCheck();
	void AddDir();

	UtilDomainInit(TRUE);
	GetDomainDirs(&list);
	MaybeKillList(&DomainMap, IntegCheck);
	RunList(&list, AddDir);
	UpdateMap();
}

static void *FindDir(char *name, int *target)
{
	if (atoi(name) == *target) return name;
	return NULL;
}


int FindHigh(int dir)
{
	int rover;
	char temp[12];
	DOMAIN_FILE buffer;

	for (rover = 0; ; rover++) {
	    sprintf(temp, "%d", rover);
	    MakeDomainFileName(buffer, dir, temp);
	    if (access(buffer, 0) != 0)
		break;
	}
	return rover;
}

int IntegCheck(DomainDir *data)
{
	if (SearchList(&list, &data->MapDir) == NULL)
		return TRUE;

	KillData(&list, &data->MapDir);
	data->HighFile = FindHigh(data->MapDir);
	return FALSE;
}

void AddDir(char *s)
{
	extern SListBase DomainMap;
	DomainDir *dir;
	DOMAIN_FILE buffer;
	FILE *fd;

	if (isdigit(*s)) {
		MakeDomainFileName(buffer, atoi(s), "info");
		if ((fd = fopen(buffer, "r")) != NULL) {
			dir = GetDynamic(sizeof *dir);
			if (GetAString(dir->Domain, sizeof dir->Domain, fd) !=
									NULL) {
				dir->MapDir = atoi(s);
				dir->HighFile = FindHigh(dir->MapDir);
				AddData(&DomainMap, dir, NULL, FALSE);
			}
			fclose(fd);
		}
	}
}

/*
 * RoomInfoIntegrity()
 *
 * This function checks the room info integrity.  If infomap does not exist,
 * it attempts to reconstruct it.
 *
 * NB: If room info data structures change, so does this function.
 */
void RoomInfoIntegrity()
{
	void InfoFn(DirEntry *str);

	if (!ReadCitInfo()) {
		printf("Attempting to reconstruct room information...\n");
		MoveToInfoDirectory();
		SysWildCard(InfoFn, "*.inf");
		homeSpace();
		WriteOutInformation();
	}
}

/*
 * InfoFn()
 *
 * This function handles an entry from the info directory.
 */
static void InfoFn(DirEntry *str)
{
	int room;
	extern SListBase InfoMap;

	room = atoi(str->unambig);
	if (room < 0 || room >= MAXROOMS) {
		printf("Info file '%s' has out of range suffix, not matched with any room.\n", str->unambig);
		return;
	}

	if (!roomTab[room].rtflags.INUSE) {
		printf("Info file '%s' matching room slot is not in use, therefore not used.\n", str->unambig);
		return;
	}
	AddData(&InfoMap, NtoStrInit(room, roomTab[room].rtname, 0, FALSE),
						NULL, FALSE);
}
