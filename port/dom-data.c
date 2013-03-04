/*
 *				Dom-data.C
 *
 * Domain handing data.
 */

#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"
#include "2ndfmt.h"

/*
 *				history
 *
 * 95Oct18 HAW  Created.
 */

/*
 *				contents
 *
 */

#define MAPSYS		"map.sys"

/*
 * Required Global Variables
 */
extern int thisNet;
extern NetBuffer netBuf, netTemp;
extern MessageBuffer   msgBuf;

/*
 * Current domain mail in #DOMAINAREA -- map of directories to domain names.
 */
void *CheckDomain(), *EatDomainLine(char *line);
int CmpDomain();

SListBase DomainMap    = { NULL, CheckDomain, CmpDomain, free, EatDomainLine };

char *DomainFlags      = NULL;

extern CONFIG    cfg;

/*
 * This code is responsible for managing the domain part of Citadel-86.
 *
 * DOMAIN DIRECTORY:
 *    The domain directory consists of a single file and a collection of
 * directories.
 *
 * MAP.SYS: this contains a mapping between the directory names and the
 * domain mail they purport to store.  Format:
 *
 * <number><space><domain><highest filename>
 *
 * DIRECTORIES (1, 2,...): these contain the actual outgoing mail, plus a file
 * named "info" which will hold the name of the domain (for rebuilding
 * purposes).
 */

/*
 * EatDomainLine()
 *
 * This function parses a line from map.sys.  Map.sys is kept in the
 * #DOMAINAREA and maps the from the name of each domain with outstanding
 * mail and the directory it resides in.  Directories are numerically
 * designated.  Format of each line is
 *
 * <directory name> <domain name> <last file number>
 *
 * This function, since it's used by calls to MakeList, returns a void pointer
 * to the assembled data structure.
 */
static void *EatDomainLine(char *line)
{
    DomainDir *data;
    char *c;

    data = GetDynamic(sizeof *data);
    data->UsedFlag   = FALSE;
    data->TargetSlot = -1;
    if ((c = strchr(line, ' ')) == NULL) {
	free(data);
	return NULL;
    }
    *c++ = 0;
    data->MapDir = atoi(line);
    line = c;
    if ((c = strchr(line, ' ')) == NULL) {
	free(data);
	return NULL;
    }
    *c++ = 0;
    strCpy(data->Domain, line);
    data->HighFile = atoi(c);
    return data;
}

char CheckNumber = FALSE;	/* double duty switcheroonie	*/
/*
 * CheckDomain()
 *
 * This is used to look for a domain in a list.  The search can either be on
 * the directory number or the domain name.
 */
static void *CheckDomain(DomainDir *d, char *str)
{
    int *i;

    if (!CheckNumber) {
	if (strCmpU(d->Domain, str) == SAMESTRING) return d;
    }
    else {
	i = (int *) str;
	if (*i == d->MapDir) return d;
    }
    return NULL;
}

/*
 * CmpDomain()
 *
 * This function is used in the list of domains with mail to keep the domains
 * in numerical order.  The list is kept in numerical order to make MAP.SYS
 * a bit more tractable.
 */
static int CmpDomain(DomainDir *s, DomainDir *t)
{
    return s->MapDir - t->MapDir;
}

#define MAPSYS		"map.sys"
#define WeServe(x)	SearchList(&Serves, x)

/*
 * Some useful static functions.
 */
void DomainLog(char *str);
void *EatCosts(char *line);
int SetUpCallOut(char *DName);
int CallOutWork(char *DName);

int lifo();

/*
 * List of domain handlers -- source: CTDLDMHD.SYS, CTDLDMHD.LCL.
 */
void *FindDomainH(), *EatDomainH();
SListBase DomainHandlers = { NULL, FindDomainH, lifo, free, EatDomainH };

SListBase Costs = { NULL, ChkStrtoN, NULL, NULL, EatCosts };
UNS_16 UnknownCost     = 1;
int RouteSlot;

extern CONFIG    cfg;
extern NetBuffer netBuf, netTemp;
extern NetTable  *netTab;
extern FILE      *upfd, *netLog;
extern int       thisNet;
extern MessageBuffer   msgBuf;
extern logBuffer logBuf;
extern label	 HomeId;
extern char      *READ_ANY, *WRITE_TEXT;
extern int	 RMcount;
extern SListBase Serves;
extern char      inNet;

/*
 * FindDomainH()
 *
 * This finds the named domain handler on the domain handler list.
 */
static void *FindDomainH(DomainHandler *data, char *name)
{
    return (strCmpU(data->domain, name) == SAMESTRING) ? data : NULL;
}

/*
 * EatDomainH()
 *
 * This eats a line from ctdldmhd.sys.  Notice the support for the 'via'
 * capability.
 */
static void *EatDomainH(char *line)
{
    void *ViaHandle(char *line);
    DomainHandler *data;
    char *c;
    label temp;

    if ((c = strchr(line, '#')) != NULL) *c = 0;	/* kill '#' */
    NormStr(line);
    if (strlen(line) <= 3) return NULL;

    /* if line is of format "domain via domain" then handle that */
    /* characterized by lack of colon */
    if ((c = strchr(line, ':')) == NULL) {
	return ViaHandle(line);
    }

    if ((c = strchr(line, ' ')) == NULL) {
	printf("WARNING: badly formed ctdldmhd.sys (1:-%s-).\n", line);
	return NULL;
    }
    *c++ = 0;

    if ((c = strchr(c, ':')) == NULL) {
	printf("WARNING: badly formed ctdldmhd.sys (2:-%s-).\n", c);
	return NULL;
    }

    c++;

    data = GetDynamic(sizeof *data);
    data->domain = strdup(line);
    normId(c, temp);
    data->nodeId = strdup(temp);
    return data;
}

/*
 * EatCosts()
 *
 * This function eats a line from ctdlcost.sys and returns a structure
 * containing the cost suitable for inclusion into a list.
 */
static void *EatCosts(char *line)
{
    NumToString *data;
    char *c;

    if ((c = strchr(line, ' ')) == NULL) return NULL;
    *c++ = 0;
    if (strCmpU(line, "Unknown") == SAMESTRING) {
	UnknownCost = atoi(c);
	return NULL;
    }
    data = GetDynamic(sizeof *data);
    data->string = strdup(line);
    data->num = atoi(c);
    return data;
}

/*
 * lifo()
 *
 * Universal function for causing Last In First Out lists to occur in the
 * SList stuff.
 */
lifo()
{
	return 1;
}

void *OnTarget(), *EatRoute(char *line);
SListBase Routes = { NULL, OnTarget, NULL, NULL, EatRoute };

/*
 * OnTarget()
 *
 * This function helps find a routing record based on the Target field.
 */
static void *OnTarget(Routing *element, int *i)
{
    if (element->Target == *i) return element;
    return NULL;
}

/*
 * EatRoute()
 *
 * This function eats a line from ROUTING.SYS.
 */
static void *EatRoute(char *line)
{
    Routing *record;
    char *target, *via;
    int  targind, viaind;

    if ((target = strchr(line, '#')) != NULL) *target = 0;
    if ((target = strtok(line, ":")) == NULL) {
	return NULL;
    }
    if ((via = strtok(NULL, ":")) == NULL) {
	return NULL;
    }
    NormStr(target);
    NormStr(via);
    if ((targind = searchNameNet(target, &netTemp)) == ERROR) {
	return NULL;
    }
    if ((viaind = searchNameNet(via, &netTemp)) == ERROR) {
	return NULL;
    }
    record = GetDynamic(sizeof *record);
    record->Target = targind;
    record->Via = viaind;
    return record;		/* checked is automatically initialized */
}


/*
 * ViaHandle()
 *
 * This handles a line of format "domain via domain" for EatDomainH().  It
 * always returns NULL since it's only setting up the link, not creating a
 * new record.
 */
static void *ViaHandle(char *line)
{

	/* we're guaranteed of no comments on line */
    char *space, *via;
    DomainHandler *target, *viadomain;

    if ((space = strchr(line, ' ')) == NULL) {
	printf("WARNING: badly formed ctdldmhd.sys (3:-%s-).\n", line);
	return NULL;
    }

    *space++ = 0;
    if (strncmp("via ", space, 4) != 0) {
	if (strncmp("is ", space, 3) != 0) {
	    printf("WARNING: badly formed ctdldmhd.sys (4:-%s-).\n", space);
	    return NULL;
	}
	/* handle "<domain> is <domain>" (aliasing domains) */
	else {
	    via = strchr(space, ' ') + 1;
	    if ((viadomain = SearchList(&DomainHandlers, via)) != NULL) {
		target = GetDynamic(sizeof *target);
		target->flags  = ALIAS_RECORD;
		target->domain = strdup(line);
		target->via = viadomain;
		return target;
	    }
	    else printf("Didn't find domain %s\n", via);
	    return NULL;
	}
    }

    /* no failure guaranteed ... */
    via = strchr(space, ' ') + 1;

    if ((target = SearchList(&DomainHandlers, line)) != NULL) {
	if ((viadomain = SearchList(&DomainHandlers, via)) != NULL) {
	    target->via = viadomain;
	}
    }

	/*
	 * since we aren't building a new record but instead are creating a link
	 * between two records, we always return NULL.
	 */
    return NULL;
}
