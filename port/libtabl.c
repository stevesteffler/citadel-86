/*
 *				libtabl.c
 *
 * Code to handle CTDLTABL.SYS
 */

/*
 *				history
 *
 * 87Jan20 HAW  Integrity stuff for portability.
 * 86Apr24 HAW  Modified for fwrite() and fread().
 * 85Nov15 HAW  Created.
 */

#include "ctdl.h"

/*
 *				Contents
 *
 *	readSysTab()		restores system state from ctdltabl.sys
 *	common_read()		bottleneck for reading
 *	writeSysTab()		saves state of system in CTDLTABL.SYS
 *	GetDynamic()		allocation bottleneck
 */

CONFIG		cfg;			/* A buncha variables	*/
LogTable	*logTab;		/* RAM index of pippuls	*/
NetTable	*netTab;		/* RAM index of nodes	*/
rTable		*roomTab;		/* RAM index of rooms	*/
EVENT		*EventTab = NULL;
char		*indexTable = "ctdlTabl.sys";
struct floor	*FloorTab;
int		TopFloor;
static char	*msg1 = "?old ctdlTabl.sys!";

static struct {
    int checkMark;			/* rudimentary integrity */
    int cfgSize;			/* sizeof cfg	*/
    int logTSize;			/* logtab size	*/
    int endMark;			/* another integrity check      */
} integrity;

extern char *R_W_ANY;

SListBase Serves     = { NULL, FindStr, NULL, free, NULL };
SListBase Moderators = { NULL, ChkNtoStr, NULL, FreeNtoStr, EatNMapStr };
			/* These two should change from major release to */
#define CHKM    8       /* major release	*/
#define ENDM    9

label HomeId;
/*
 * readSysTab()
 *
 * This function restores the state of system from CTDLTABL.SYS
 * returns:		TRUE on success, else FALSE
 * destroys CTDLTABL.TAB after read, to prevent erroneous re-use
 * in the event of a crash.
 *
 * MS-DOS fun: Here's the map --
 * Word 1 == sizeof cfg 
 * Word 2 == sizeof logTab
 * Word 3 == sizeof roomTab
 * Word 4 -- thru x == cfg contents
 * x -- y == logTab
 * y -- z == roomTab
 * z -- a == netTab
 * EOF
 */
char readSysTab(char kill, char showMsg)
{
    FILE	*fd;
    extern char *READ_ANY;
    int		rover;
    long	bytes;
    SYS_FILE    name;
    char	caller;
    label       temp;

    caller = cfg.weAre;

    if ((fd = fopen(indexTable, READ_ANY)) == NULL) {
	if (showMsg) printf("?no %s!", indexTable);    /* Tsk, tsk! */
	return(FALSE);
    }

    if (fread(&integrity, sizeof integrity, 1, fd) != 1) {
	if (showMsg) printf(msg1);
	return FALSE;
    }

    if (     integrity.checkMark != CHKM ||
	     integrity.endMark != ENDM ||
	     integrity.cfgSize != sizeof cfg) {
	if (showMsg) printf(msg1);
	return(FALSE);
    }

    if (!common_read(&cfg, (sizeof cfg), 1, fd, showMsg))
	return FALSE;

		/* Allocations for dynamic parameters */
    logTab = (LogTable *) GetDynamic(integrity.logTSize);

    roomTab = (rTable *) GetDynamic(MAXROOMS * (sizeof (*roomTab)));

    if (cfg.netSize)
	netTab = (NetTable *) GetDynamic(sizeof (*netTab) * cfg.netSize);
    else
	netTab = NULL;

    if (cfg.EvNumber)
	EventTab  = (EVENT *)
			GetDynamic(sizeof (*EventTab) * cfg.EvNumber);

				/* "- 1" is kludge */
    if (integrity.logTSize != sizeof (*logTab) * cfg.MAXLOGTAB) {
	if (showMsg) printf(msg1);
	return(FALSE);
    }

    if (!common_read(logTab, integrity.logTSize, 1, fd, showMsg))
	return FALSE;

    if (!common_read(roomTab, (sizeof (*roomTab)) * MAXROOMS, 1, fd, showMsg))
	return FALSE;

    if (cfg.netSize) {
	for (rover = 0; rover < cfg.netSize; rover++) {
	    if (!common_read(&netTab[rover], NT_SIZE, 1, fd, showMsg))
		return FALSE;
	}
    }

    if (cfg.EvNumber) {
	if (!common_read(EventTab, (sizeof(*EventTab) * cfg.EvNumber), 1, fd,
								showMsg))
	    return FALSE;
    }

    for (rover = 0; rover < cfg.DomainHandlers; rover++) {
	if (!common_read(temp, NAMESIZE, 1, fd, showMsg)) {
	    return FALSE;
	}
	AddData(&Serves, strdup(temp), NULL, FALSE);
    }

    fclose(fd);

    makeSysName(name, "ctdlflr.sys", &cfg.floorArea);

    if ((fd = fopen(name, R_W_ANY)) == NULL) {
	if (caller != CONFIGUR) {
	    if (showMsg) printf("No floor table!");
	    return FALSE;
	}
    }
    else {
	totalBytes(&bytes, fd);
	FloorTab = (struct floor *) GetDynamic((int) bytes);
	if (fread(FloorTab, (int) bytes, 1, fd) != 1) {
	    if (showMsg) printf("problem reading floor tab");
	    fclose(fd);
	    if (caller != CONFIGUR) return FALSE;
	}
	else {
	    fclose(fd);
	    TopFloor = (int) bytes/sizeof(*FloorTab);
	}
    }

    if (kill) unlink(indexTable);

    crypte(cfg.sysPassword, sizeof cfg.sysPassword, 0);

    normId(cfg.codeBuf + cfg.nodeId, HomeId);

    return(TRUE);
}

/*
 * common_read()
 *
 * This function reads in from file the important stuff.
 * returns:	TRUE on success, else FALSE
 */
static int common_read(void *block, int size, int elements, FILE *fd,
								char showMsg)
{
    if (size == 0) return TRUE;
    if (fread(block, size, elements, fd) != 1) {
	if (showMsg) printf(msg1);
	return FALSE;
    }
    return TRUE;
}

/*
 * writeSysTab()
 *
 * This saves state of system in CTDLTABL.SYS
 * returns:	TRUE on success, else ERROR
 * See readSysTab() to see what the CTDLTABL.SYS map looks like.
 */
int writeSysTab()
{
    void WriteServers();
    int	rover;
    FILE *fd;
    extern char   *WRITE_ANY;

    if ((fd = fopen(indexTable, WRITE_ANY)) == NULL) {
	printf("?can't make %s", indexTable);
	return(ERROR);
    }

    /* Write out some key stuff so we can detect bizarreness: */
    integrity.checkMark = CHKM;
    integrity.endMark = ENDM;
    integrity.cfgSize = sizeof cfg;
    integrity.logTSize = sizeof (*logTab) * cfg.MAXLOGTAB;

    fwrite(&integrity, (sizeof integrity), 1, fd);

    crypte(cfg.sysPassword, sizeof cfg.sysPassword, 0);
    fwrite(&cfg, (sizeof cfg), 1, fd);
    crypte(cfg.sysPassword, sizeof cfg.sysPassword, 0);

    fwrite(logTab, (sizeof(*logTab) * cfg.MAXLOGTAB), 1, fd);
    fwrite(roomTab, (sizeof (*roomTab)) * MAXROOMS, 1, fd);
    for (rover = 0; rover < cfg.netSize; rover++) {
	fwrite(&netTab[rover], NT_SIZE, 1, fd);
    }
    if (cfg.EvNumber)
	fwrite(EventTab, (sizeof(*EventTab) * cfg.EvNumber), 1, fd);

    RunListA(&Serves, WriteServers, (void *) fd);

    fclose(fd);
    return(TRUE);
}

/*
 * WriteServers()
 *
 * This function writes a domain server out to ctdltabl.sys.  See DOMAINS.C
 * for more information on this list.
 */
static void WriteServers(char *name, FILE *fd)
{
    fwrite(name, NAMESIZE, 1, fd);
}

/* #define MORE_DEBUG */
/*
 * GetDynamic()
 *
 * This does mallocs with error checking.
 */
void *GetDynamic(unsigned size)
{
    void *temp;
    void *malloc();
#ifdef MORE_DEBUG
    unsigned long coreleft(void);
    char msg[80];
#endif

    if (size == 0) return NULL; /* Simplify code	*/
    temp = malloc(size);
 /* printf("Requested %d bytes, received address %p\n", size, temp); */
    if (temp == NULL) {
	printf("Request for %u bytes failed.\n", size);
#ifdef MORE_DEBUG
	sprintf(msg, "Asked for %u bytes.  CoreLeft=%ld.\n", size, coreleft());
	crashout(msg);
#else
	crashout("Memory failure -- I need more memory!");
#endif
    }
    memset(temp, 0, size);
    return temp;
}

/*
 * openFile()
 *
 * This opens one of the .sys files.
 */
void openFile(char *filename, FILE **fd)
{
	/* We use fopen here rather than safeopen for link reasons */
    if ((*fd = fopen(filename, R_W_ANY)) == NULL) {
	printf("?no %s", filename);
	exit(SYSOP_EXIT);
    }
}

/*
 * FindStr()
 *
 * This is a find the server function.  It's used in the list of domain servers
 * to allow us to search for a domain server based on the name of the domain.
 *
 * Actually implemented as "FindStr" so we can use it in other contexts.  It's
 * just a simple search for string in list of strings function.
 */
void *FindStr(char *name, char *target)
{
    return (strCmpU(name, target) == SAMESTRING) ? name : NULL;
}

