/*
 *				Domains.C
 *
 * Domain handing code.
 */

#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"
#include "2ndfmt.h"

/*
 *				history
 *
 * 90May20 HAW  Created.
 */

/*
 *				contents
 *
 *	DomainInit()		Initializes domains.  Call once only.
 *	EatDomainLine()		Eats a line from map.sys.
 *	CheckDomain()		Helps search lists for a domain.
 *	CmpDomain()		Helps sort a list of domains.
 *	DomainMailFileName()	Creates a filename for mail for a domain.
 *	GetDomain()		Create a domain entry for domain mail.
 *	UpdateMap()		Updates Map.sys.
 *	WriteDomainMap()	Helps write out Map.sys.
 *	DomainFileAddResult()	Finishes dealing with a piece of domain mail.
 *	RationalizeDomains()	Checks all active domains for integrity.
 *	DomainRationalize()	Checks a single domain for integrity.
 *	KillDomain()		Takes a domain out of list and disk.
 *	FindDomainH()		Helps find a domain handler in a list.
 *	EatDomainH()		Eats a line from ctdldmhd.sys.
 *	ViaHandle()		Handles a 'via' line from ctdldmhd.sys.
 *	HandleExistingDomain()	Process a domain we serve.
 *	SetCallout()		Sets callout flags depending on domains.
 *	SetUpCallOut()		Finds direction to sling domain mail.
 *	CallOutWork()		Work fn to find direction ...
 *	ClearSearched()		Clears a search marker for each domain.
 *	DomainOut()		Manages transmission of domain mail.
 *	SendDomainMail()	Does actual work of transmitting domain mail.
 *	EatCosts()		Eat a line from ctdlcost.sys.
 *	FindCost()		Helps find the cost for a given domain.
 *	WriteDomainContents()	Handles .EN? in Mail.
 *	DomainLog()		Handles the file DOMAIN.LOG.
 *	RouteHere()		Is domain mail meant for here?
 *	LocalName()		Is given system local?
 *	lifo()			Last in First Out fn for the lists.
 */

#define MAPSYS		"map.sys"
#define WeServe(x)	SearchList(&Serves, x)

/*
 * Some useful static functions.
 */
void *FindDomainH(), *EatDomainH();
void DomainLog(char *str);
void *CheckDomain(), *EatDomainLine(char *line);
int CmpDomain();
int SetUpCallOut(char *DName);
int CallOutWork(char *DName);

/*
 * Required Global Variables
 */

/*
 * DomainDir
 *
 * Variables of this sort define the mapping between a domain name and the
 * directory containing outstanding mail bound for that domain.  Typically,
 * a domain with no mail will not have an allocated variable.
 *
 * UsedFlag - a temporary flag indicating that domain mail was sent during
 * the last network connection.  Used for performance reasons.
 *
 * Domain - The name of the domain represented by this record.
 *
 * MapDir - The "name" of the directory containing the mail for this domain.
 * This is a number; each domain is given a unique number amongst those
 * active, starting with 0.  When access to the mail is desired, this number
 * is used to form the name.
 *
 * HighFile - The next file name (also formed from numbers starting at 0) to
 * use for incoming mail for this domain.
 *
 * TargetSlot - Index into CtdlNet.Sys indicating the system to pass this mail
 * to.  When we're connected with a system and its call out flags indicate
 * domain mail is bound for it, we use this flag to quickly find if this
 * domain is bound for the current node connection.  The flag is set when
 * the system initially comes up (or the record is created) by checking
 * against the contents of CTDLDMHD.SYS.
 */
DomainDir *GetDomain(char *DName, char create);


/*
 * List of domain handlers -- source: CTDLDMHD.SYS, CTDLDMHD.LCL.
 */
extern SListBase DomainHandlers;

/*
 * List of costs for domains (from ctdlcost.sys).
 */
extern SListBase Costs;

/*
 * Current domain mail in #DOMAINAREA -- map of directories to domain names.
 */
extern SListBase DomainMap;
extern char *DomainFlags;
/* UNS_16 UnknownCost     = 1; */

extern CONFIG    cfg;
extern NetBuffer netBuf, netTemp;
extern NetTable  *netTab;
extern FILE      *upfd, *netMisc, *netLog;
extern int       thisNet;
extern MessageBuffer   msgBuf;
extern logBuffer logBuf;
extern label	 HomeId;
extern char      *READ_ANY, *WRITE_TEXT;
extern int	 RMcount;
extern SListBase Serves;
extern char      inNet;

void ClearSearched();
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
 * DomainOut()
 *
 * This function is tasked with sending all domain mail to the current system
 * we're connected to that is destined for that system.  Actually, it just
 * cycles through all current domains and lets another function handle the
 * real work.
 */
int DomainOut(char LocalCheck)
{
	char TransmitDomainMail(char *name, char *domain, char LocalCheck);

	return UtilDomainOut(TransmitDomainMail, LocalCheck);
}

char TransmitDomainMail(char *name, char *domain, char LocalCheck)
{
	label Id, Name;
	int result;

	if (!gotCarrier()) return STOP_TRAVERSAL;

	result = SendRouteMail(name, domain, Id, Name, LocalCheck);
	if (result == REFUSED_ROUTE) {
		sprintf(msgBuf.mbtext,"Domain mail: %s _ %s rejected by %s.",
					Name, domain, netBuf.netName);
		netResult(msgBuf.mbtext);
	}
	return result;
}

/*
 * WriteDomainContents()
 *
 * This writes the secondary list in readable format by reading NODES*.RAW.
 */
void WriteDomainContents()
{
    int rover;
    SYS_FILE secondary;
    label name;
    int   count = 0;
    char  line[80], vers[VERS_SIZE + 1];
    char  work[80], *c;
    FILE *fd, *fd2;
    extern char *READ_TEXT;

    for (rover = 0; rover < 100; rover++) {
	sprintf(name, "nodes%d.raw", rover);
	makeSysName(secondary, name, &cfg.netArea);
	if ((fd = fopen(secondary, READ_TEXT)) == NULL) {
	    break;
	}

	/* make sure the raw version genned the fst version */
	sprintf(name, "nodes%d.fst", rover);
	makeSysName(secondary, name, &cfg.netArea);
	if ((fd2 = fopen(secondary, READ_ANY)) == NULL) {
	    fclose(fd);
	    continue;
	}

	GetAString(line, 80, fd);	/* gets version line */
	fread(vers, 1, VERS_SIZE, fd2);
	vers[VERS_SIZE] = 0;

	fclose(fd2);
	if (strCmpU(line, vers) != SAMESTRING) {
	    fclose(fd);
	    continue;
	}

	while (GetAString(line, 80, fd) != NULL) {
	    if (strncmp(line, ".domain", 7) == SAMESTRING) {
		if (count != 0) doCR();
		strCpy(work, strchr(line, ' ') + 1);
		if ((c = strchr(work, ' ')) != NULL) *c = 0;
		doCR();
		count = 0;
		mPrintf("In domain %s", strchr(line, ' ') + 1);
		if (FindCost(work) != 0)
		    mPrintf(" (%d LD credits):", FindCost(work));
		doCR();
	    }
	    else {
		if (strchr(line, ':') != NULL) continue;
		NormStr(line);
		if (strLen(line) == 0) continue;
		mPrintf("%s", line);
		if (++count % 3 == 0) {
		    count = 0;
		    doCR();
		}
		else {
#ifdef TURBO_C_VSPRINTF_BUG
		    SpaceBug(28 - strLen(line));   /* EEEESH */
#else
		    mPrintf("%*c", 28 - strLen(line), ' ');
#endif
		}
	    }
	}
	if (count != 0) doCR();
	doCR();
	fclose(fd);
    }
}

