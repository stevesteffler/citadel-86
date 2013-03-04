/*
 *				Dom-core.C
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
 *	SystemInSecondary()	Check for name in secondary list(s).
 *	SearchSecondary()	Work fn for above.
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
void DomainLog(char *str);
void *EatCosts(char *line);
int SetUpCallOut(char *DName);
int CallOutWork(char *DName);

/*
 * Required Global Variables
 */

DomainDir *GetDomain(char *DName, char create);

int lifo();

/*
 * List of costs for domains (from ctdlcost.sys).
 */
extern SListBase Costs;

/*
 * List of domain handlers -- source: CTDLDMHD.SYS, CTDLDMHD.LCL.
 */
extern SListBase DomainHandlers;

/*
 * Current domain mail in #DOMAINAREA -- map of directories to domain names.
 */
extern SListBase DomainMap;
extern char *DomainFlags;
extern UNS_16 UnknownCost/*     = 1 */;
extern int RouteSlot;

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
 * DomainMailFileName()
 *
 * This function is called when FR 9 has been received: someone wants to send
 * us routemail with a domain attached.  We are responsible for creating a
 * good filename for it, setting flags etc.  Return REFUSE if we can't accept
 * mail (for fatal or other reasons), LOCALROUTE if we are the domain server
 * and we know the target, OURS if we are the target, DOMAINFILE if we need
 * to store the file in a domain directory.
 * NB: the caller of this function provides security.
 * See DomainFileResult().
 * NB: this can also be called for local users sending domain mail
 */
char DomainMailFileName(DOMAIN_FILE buffer, label DName, label NodeId,
						label NodeName)
{
    DomainDir *data;
    char temp[10];

    /*
     * The first clause of this if statement is necessary for the following
     * scenario.  If we're in net mode, suppose the caller is conveying
     * domain mail which, as it happens, it will be the route for the
     * final leg of the mail.  (Yes, this actually happened in practice --
     * an LD node was accessible only via a local node to the hub.)  Now,
     * if this if isn't structured this way, when the incoming domain mail
     * is deciphered, it will be assigned to this node WHILE the call is
     * active, including the activation of the mail routing flag.  When the
     * call terminates, these same flags will be turned OFF.  Net result
     * (if you'll pardon the pun) is that R*.* files will be left in #NETAREA
     * and will NOT be delivered.  Tsk tsk.  Therefore, in doResults(),
     * a call to RationalizeDomains() is made which will clean up the domain
     * directories after a call.
     */
    DName = RealDomainName(DName);

    if (inNet == NON_NET && WeServe(DName) != NULL) {
	/* since we're server, we don't search the secondary lists */
	if (FindTheNode(NodeId, NodeName)) {
	    if (netTemp.nbflags.RouteTo) {
		strCpy(NodeId, netTemp.netId);
		return LOCALROUTE;
	    }
/* else printf("RouteTo is off\n"); */
	}
/* else printf("FindTheNode(%s, %s) failed\n", NodeId, NodeName); */
	/*
	 * don't return if either conditional fails.  if we don't know
	 * about the node, still accept the mail for delivery, and see
	 * the use of HandleExistingDomain(), which is called each time
	 * the system is brought up.  If it's a node we know about but
	 * don't like, accept the mail anyways -- just in case we changes
	 * our mindses.  if we don't, well, hey, too bad.
	 */
    }
/* else printf("inNet == %d, WeServe returns %lx\n", inNet, WeServe(DName)); */

    /*
     * rare, but possible case - mail meant for us but we don't serve the
     * domain, it's just getting routed via us.  this also picks up all
     * cases where domain mail is being sent to the domain-server.
     */
    NormStr(NodeId);
    if (RouteHere(NodeId, NodeName, DName)) {
	return OURS;
    }

    /* ok, we need to store it in its proper domain */
    data = GetDomain(DName, TRUE);	/* GetDomain() creates, etc.	*/
    sprintf(temp, "%d", data->HighFile++);
    MakeDomainFileName(buffer, data->MapDir, temp);
    UpdateMap();
    return DOMAINFILE;
}


static FILE *fd;
/*
 * GetDomain()
 *
 * This function is used to find the given domain in the installation's
 * current outstanding mail.  If no such domain exists, then create the
 * data structure and the directory entry.  In either case, return a pointer
 * to the appropriate record.  This should never return NULL.
 */
DomainDir *GetDomain(char *DName, char create)
{
    extern char CheckNumber;
    DomainDir *data;
    int i;
    DOMAIN_FILE buf;

    if ((data = SearchList(&DomainMap, DName)) == NULL) {
	if (!create) return NULL;
	/* create it */
	data = GetDynamic(sizeof *data);
	data->UsedFlag = FALSE;
	strCpy(data->Domain, DName);
	data->HighFile = 0;
	CheckNumber = TRUE;
	for (i = 0; i < 10000; i++)
	    if (SearchList(&DomainMap, &i) == NULL) break;
	data->MapDir = i;
	AddData(&DomainMap, data, NULL, FALSE);
	MakeDomainDirectory(i);
	MakeDomainFileName(buf, data->MapDir, "info");
	if ((fd = fopen(buf, WRITE_TEXT)) != NULL) {
	    fprintf(fd, "%s", DName);
	    fclose(fd);
	}
	CheckNumber = FALSE;
 	/* UpdateMap();	   don't update -- let caller do it */
    }
    return data;
}

/*
 * DomainFileAddResult()
 *
 * This function handles the result of a domain file addition.  This is paired
 * with DomainMailFileName() and should always be called after it has been
 * used to give out results.
 */
void DomainFileAddResult(char *DName, label system, label NodeId, char result)
{
    DomainDir *data;
    DOMAIN_FILE buf;
    char work[100];

    DName = RealDomainName(DName);
    if ((data = GetDomain(DName, FALSE)) == NULL) {
	printf("URP IN DOMAINS!");
	return;
    }

    if (result == DOMAIN_SUCCESS) {
	if ((data->TargetSlot = SetUpCallOut(DName)) == ERROR) {
	    if (WeServe(DName) != NULL) {
	    /* since we're server, we don't search the secondary lists */
		if (!FindTheNode(NodeId, system)) {
		    sprintf(work, "Mail for unknown system %s.", system);
		    DomainLog(work);
		}
	    }
	    else if (data->HighFile == 1) {
		sprintf(work, "Don't know how to reach domain %s.", DName);
		DomainLog(work);
	    }
	}
	return;
    }

    /* failure -- assume caller erases file */
    if (--data->HighFile == 0) {	/* this indicates empty dir */
	MakeDomainFileName(buf, data->MapDir, "info");
	unlink(buf);
	KillDomainDirectory(data->MapDir);
	KillData(&DomainMap, data->Domain);	/* data's ptr is now invalid */
    }
    UpdateMap();
}

void KillDomain();
static SListBase KillDomains = { NULL, NULL, NULL, KillDomain, NULL };
static char Change;
/*
 * RationalizeDomains()
 *
 * This function goes through all the domains checking for empty domains and
 * domains with file lists with "holes" in them (caused by a node rejecting
 * some routemail -- won't happen in C-86 [probably], but there's nothing in
 * the rules to say it can't happen).  This should be called after each network
 * call involving domain-mail transmissions.
 */
void RationalizeDomains()
{
    void DomainRationalize();
    void SetCallout();

    Change = FALSE;
    RunList(&DomainMap, DomainRationalize);

    KillList(&KillDomains);	/* clears list and kills empty domains, too */

    if (Change) UpdateMap();

    UtilDomainInit(FALSE);		/* reset temp call out flags */
    RunList(&DomainMap, SetCallout);
}

/*
 * DomainRationalize()
 *
 * See RationalizeDomains for detail.
 *
 * NB: this could be made far more efficient someday by keeping track of the
 * last gap, or keeping a list of successfully sent mail files, or ... ?
 */
void DomainRationalize(DomainDir *data)
{
    int rover, rover2, lasthit = -1;
    char temp[10], temp2[10];
    DOMAIN_FILE buffer, buffer2;

    if (data->UsedFlag) {
	data->UsedFlag = FALSE;
	Change = TRUE;
	for (rover = 0; rover < data->HighFile; rover++) {
	    sprintf(temp, "%d", rover);
	    MakeDomainFileName(buffer, data->MapDir, temp);
	    if (access(buffer, 0) != 0) {	/* hole found */
		for (rover2 = rover + 1; rover2 < data->HighFile; rover2++) {
		    sprintf(temp2, "%d", rover2);
		    MakeDomainFileName(buffer2, data->MapDir, temp2);
		    if (access(buffer2, 0) == 0) {	/* end of gap */
			rename(buffer2, buffer);	/* rename file */
			lasthit = rover;
		    }
		}
	    }
	    else lasthit = rover;
	}

	if (lasthit == -1) {
	    MakeDomainFileName(buffer, data->MapDir, "info");
	    unlink(buffer);
	    KillDomainDirectory(data->MapDir);
	    /* we can't kill the data right now 'cuz we're in list processing */
	    /* so we'll store it and kill it later */
	    AddData(&KillDomains, &data->MapDir, NULL, FALSE);
	}
	else data->HighFile = lasthit + 1;
    }
}

/*
 * KillDomain()
 *
 * Kills a domain from DomainMap.
 */
static void KillDomain(int *MapDir)
{
    extern char CheckNumber;

    CheckNumber = TRUE;
    KillData(&DomainMap, MapDir);	/* KABOOM! */
    CheckNumber = FALSE;
}

/*
 * SetUpCallOut()
 *
 * This is responsible for discovering what system should be called for
 * delivering mail to the designated domain.  It returns the index of the
 * node to receive the domain mail (whether final destination or simply a
 * transshipper).  If we serve the domain then return Error.
 *
 * NB: This is an interface function to the routines which actually handle
 * setting the proper call out flags and returning the proper index.  The
 * function itself initializes the data structure for the search (by
 * resetting flags in the DomainHandlers list) and then calling something
 * else to conduct the search.  This structure is called for due to the
 * recursive nature of the search.
 */
int SetUpCallOut(char *DName)
{

    DName = RealDomainName(DName);
    RunList(&DomainHandlers, ClearSearched);	/* clears searched flag */
    return CallOutWork(DName);		/* do possibly recursive work	*/
}

/*
 * CallOutWork()
 *
 * This does actual work of SetUpCallOut().  This is separated from the
 * calling function because we want to modularize the actual search ability
 * from the callers, but we must clear the search flags of the list once.
 * Since recursion is employed to follow the links of the 'via' pointers,
 * we are forced to this measure.
 *
 * Oh, yes.  This function searches for the domain handler responsible for
 * the given domain.  If not found, we use the MailHub designee as the
 * recipient of this domain mail.  If found, it then tries to find if we
 * connect directly with that server.  If so, we set the domain mail call
 * out flag for that node and return the node's index to the caller.  If we
 * don't connect directly (i.e., node is not in the list), then we try to
 * follow the 'via' pointers until a connection is found.  If never found,
 * then use the MailHub, otherwise use and return the found node.
 *
 * If we serve the domain, return ERROR.
 *
 * We use the search flag in each element of the DomainHandler's list to
 * prevent "infinite recursion" syndrome.
 */
static int CallOutWork(char *DName)
{
    DomainHandler *Domain;
    UNS_16 Slot;
    extern int RouteSlot;

    if (WeServe(DName) != NULL) return ERROR;

    if ((Domain = SearchList(&DomainHandlers, DName)) != NULL) {
	if ((RouteSlot = searchNet(Domain->nodeId, &netTemp)) != ERROR &&
		/* 120.693 */ !NodeDisabled(RouteSlot)) {
	    DomainFlags[Slot = RoutingNode(RouteSlot)] = TRUE;
	    return Slot;	/* and get out now */
	}
    }

	/*
	 * if we've searched this node before then we're in a loop and had
	 * better stop.  We handle the if clause this way as most efficient.
	 */
    if (Domain != NULL && !Domain->searched) {
	/* if a "via" is known, trace it */
	Domain->searched = TRUE;	/* infinite loop vaccine */
	if (Domain != NULL && Domain->via != NULL) {
	    return CallOutWork(Domain->via->domain);	/* recurse it */
	}
    }
else if (Domain != NULL && Domain->searched) {
printf("Loop detected\n");
}

    /* unknown hub or domain -> mailhub */
    if (cfg.MailHub != 0) {
	if ((RouteSlot = searchNameNet(cfg.codeBuf + cfg.MailHub, &netTemp))
							 != ERROR) {
	    DomainFlags[Slot = RoutingNode(RouteSlot)] = TRUE;
	    return Slot;	/* and get out now */
	}
    }

    return ERROR;
}

/*
 * SystemInSecondary()
 *
 * This looks for the given system in the secondary lists.  If a domain
 * was specified then just return almost immediately.
 */
char SystemInSecondary(char *Name, char *Domain, char *dup)
{
    int rover;
    char *sep;
    SYS_FILE secondary;
    label name;
    char WorkName[(NAMESIZE * 2) + 1];
    char SearchSecondary(char *secondary,char *Name,char *Domain,char *isdup);

    strCpy(WorkName, Name);
    /* is the domain specified already?  if so, parse it */
    if ((sep = strchr(WorkName, '_')) != NULL ||
	(sep = strchr(WorkName, '.')) != NULL) {
	*sep++ = 0;
	NormStr(WorkName);
	NormStr(sep);
	if (strchr(sep, '_') != NULL ||
	    strchr(sep, '.') != NULL)
	    return FALSE;	/* no subdomains */
	if (strLen(sep) < 1) return FALSE;
	strCpy(Name, WorkName);
	strCpy(Domain, sep);
	*dup = FALSE;		/* by definition */
	return TRUE;
    }

    Domain[0] = 0;
    for (rover = 0; rover < 100; rover++) {
	sprintf(name, "nodes%d.fst", rover);
	makeSysName(secondary, name, &cfg.netArea);
	if (access(secondary, 0) != 0) break;
	if (SearchSecondary(secondary, Name, Domain, dup)) break;
    }

    strCpy(WorkName, Name);
    /* make sure we found something and it's not us */
    return (Domain[0] != 0 &&
		strCmpU(Name, cfg.codeBuf + cfg.nodeName) != SAMESTRING &&
		strCmpU(UseNetAlias(WorkName,FALSE), cfg.codeBuf + cfg.nodeName)
				!= SAMESTRING);
}

/*
 * SearchSecondary()
 *
 * This does the actual work of searching a secondary list for a system.
 */
static char SearchSecondary(char *secondary,char *Name,char *Domain,char *isdup)
{
    FILE *fd;
    int  bucket;
    char found, *tab, *c, *tab2;
    char line[90];
    JumpInfo JumpTable[BUCKETCOUNT];

    if ((fd = fopen(secondary, READ_ANY)) == NULL)
	return FALSE;

    fread(line, VERS_SIZE + 1, 1, fd);

    fread(JumpTable, sizeof JumpTable, 1, fd);

#ifdef IS_MOTOROLA
    for (bucket = 0; bucket < BUCKETCOUNT; bucket++)
	Intel32ToMotorola(&JumpTable[bucket].offset);
#endif

    bucket = (isdigit(Name[0])) ? Name[0] - '0' :
		     toUpper(Name[0]) - 'A' + 10;

    fseek(fd, JumpTable[bucket].offset, 0);

    found = FALSE;
    do {
	*isdup = FALSE;
	if (fgets(line, sizeof line, fd) == NULL) break;
	if ((tab2 = strchr(line, '\n')) != NULL)
	    *tab2 = 0;
	if (strlen(line) == 0) {
	    break;
	}
	if (line[0] <= ' ') {
	    switch (line[0]) {
	    case DUP:
		*isdup = TRUE;
		break;
	    default: printf("Ooop!");
		break;
	    }
	    c = line + 1;
	}
	else c = line;
	tab = strchr(c, '\t');
	*tab++ = 0;
	if (strCmpU(c, Name) == 0)
	    found = TRUE;
	if (strCmpU(c, Name) > 0) break;
    } while (!found);

    if (found) {
	if ((tab2 = strchr(tab, '\t')) != NULL)
	    *tab2++ = 0;
	strCpy(Domain, tab);
	if (tab2 != NULL)    /* alias?  Copy it into search string */
	    strCpy(Name, tab2);
    }

    fclose(fd);
    return found;
}

/*
 * FindCost()
 *
 * This finds the cost of sending mail to a given domain.
 */
UNS_16 FindCost(char *domain)
{
    UNS_16 *data;

    if ((data = SearchList(&Costs, domain)) == NULL)
	return UnknownCost;
    return *data;
}

/*
 * DomainLog()
 *
 * This writes out a message to the domain log.
 */
static void DomainLog(char *str)
{
    SYS_FILE name;
    char work[200];
    extern SListBase Errors;

    sprintf(work, "(%s %s) %s", formDate(), Current_Time(), str);
    makeSysName(name, "domain.log", &cfg.domainArea);
    CallMsg(name, work);
    if (inNet == NON_NET) {
	AddData(&Errors, strdup(str), NULL, FALSE);
    }
    else
	netResult(str);		/* netResult() will add time/date stuff */
}

/*
 * RouteHere()
 *
 * Returns true if the mail is meant for this installation.
 */
char RouteHere(char *Id, char *Name, char *Domain)
{
    label temp1;
    char *c;
    extern label HomeId;

    normId(Id, temp1);
    if (strCmpU(temp1, HomeId) == SAMESTRING) return TRUE;
    if (!normId(Id, temp1)) {
	strCpy(temp1, Name);
	for (c = temp1; *c; c++)
	    if (*c == '_') *c = ' ';

	if ((Domain == NULL ||
	     strCmpU(Domain, cfg.codeBuf + cfg.nodeDomain) == SAMESTRING ||
	     WeServe(Domain) != NULL) &&
		(strCmpU(cfg.codeBuf + cfg.nodeName, Name) == SAMESTRING ||
		 strCmpU(cfg.codeBuf + cfg.nodeName,
				UseNetAlias(Name, FALSE)) == SAMESTRING))
	    return TRUE;
    }
    return FALSE;
}

/*
 * LocalName()
 *
 * This function takes a string of form <system> _ <domain> and attempts to
 * discover if this domain mapped system is actually a local.  This is used
 * when we're sending mail and are trying to find out if a Who Else override
 * needs to be generated.  Ugly	kludge, but, hey, that's what programming's
 * all about, eh?
 */
char *LocalName(char *system)
{
    char *domain, *System;

    if ((domain = strchr(system, '_')) == NULL) return system;
    domain += 2;	/* always preceded by a space -- or so we assume */

    if (strCmpU(domain, cfg.codeBuf + cfg.nodeDomain) == SAMESTRING ||
	     WeServe(domain) != NULL) {
	System = strdup(system);
	if ((domain = strchr(System, ' ')) == NULL)
	    return system;	/* should never happen, though */
	*domain = NULL;
	if (searchNameNet(System, &netTemp) != ERROR) {
	    free(System);
	    return netTemp.netName;
	}
	free(System);
    }
    return system;
}

/*
 * RealDomainName()
 *
 * This gets the real name of a domain, instead of just using an alias.
 */
char *RealDomainName(char *name)
{
    DomainHandler *Domain;
    char *incase = name;

    RunList(&DomainHandlers, ClearSearched);	/* clears searched flag */
    if ((Domain = SearchList(&DomainHandlers, name)) != NULL) {
	do {
	    if (!(Domain->flags & ALIAS_RECORD)) return Domain->domain;
	    if (Domain->searched)
		return Domain->domain;	/* no choice, but I don't like it */
	    Domain->searched = TRUE;
	    Domain = Domain->via;
	} while (Domain != NULL);
    }
    return incase;
}

/*
 * ClearSearched()
 *
 * Work function to clear the search field of a domain handler.
 */
void ClearSearched(DomainHandler *Domain)
{
    Domain->searched = FALSE;
}

/*
 * FindTheNode()
 *
 * This function will see if the node exists.  It will leave the index in
 * RouteSlot and return TRUE if found, FALSE otherwise.  This does not try
 * to utilize the secondary node lists.
 */
char FindTheNode(char *id, char *name)
{
    if ((RouteSlot = searchNet(id, &netTemp)) != ERROR) return TRUE;
    if (strLen(name) == 0) return FALSE;
    if ((RouteSlot = searchNameNet(name, &netTemp)) == ERROR &&
	(RouteSlot = searchNameNet(UseNetAlias(name,FALSE), &netTemp))==ERROR)
	return FALSE;
    return TRUE;
}

extern SListBase Routes;
/*
 * RoutingNode(int slot)
 *
 * This function finds the routing node for the given system slot, if any.
 */
int RoutingNode(int slot)
{
    Routing *record;

    if ((record = SearchList(&Routes, &slot)) != NULL) {
	return record->Via;
    }
    return slot;
}

extern SListBase Costs;
/*
 * DomainInit()
 *
 * This function initializes the domain stuff.  It's called from NetInit(),
 * addNetNode(), and from RationalizeDomains, so it has to open the relevant
 * files only once.
 *
 * Appropriate lists are created, call out flags set, etc.
 */
void DomainInit(char FirstTime)
{
    void HandleExistingDomain(), SetCallout();
    SYS_FILE temp;

    UtilDomainInit(FirstTime);
    if (FirstTime) {

	makeSysName(temp, "ctdlcost.sys", &cfg.netArea);
	MakeList(&Costs, temp, NULL);

	makeSysName(temp, "ctdldmhd.sys", &cfg.netArea);
	MakeList(&DomainHandlers, temp, NULL);

	makeSysName(temp, "ctdldmhd.lcl", &cfg.netArea);
	MakeList(&DomainHandlers, temp, NULL);

	/* now we should scan for any domain we serve, and if any are there */
	/* we need to arrange to send that mail to their recipients.	*/
	RunList(&DomainMap, HandleExistingDomain);

	RationalizeDomains();
    }

    RunList(&DomainMap, SetCallout);
}

/*
 * SetCallout()
 *
 * For some domain with outstanding mail, find out what system we'll be
 * sending that mail to and setup that value.
 */
void SetCallout(DomainDir *data)
{
    data->TargetSlot = SetUpCallOut(data->Domain);
}


/*
 * HandleExistingDomain()
 *
 * This is called to check to see if a domain with outstanding mail happens to
 * be served by this installation.  If so, then that mail must be processed:
 * mail for this system delivered to the designated users, mail for other
 * systems setup to be delivered to them if they can be identified.
 */
void HandleExistingDomain(DomainDir *dir)
{
    extern SListBase Serves;
    label	 temp;
    int		 rover;
    char	 *domain;
    DOMAIN_FILE  buffer;
    SYS_FILE     newfn;
    extern FILE  *netMisc;
    label	 nodeId, nodeName;
    char	 *c, oldNet;
    extern char  RCount, SCount, *R_W_ANY;
    extern char  inNet;
    extern int   RouteSlot;

    /*
     * we save the inNet state because this code may be called during a
     * non-netting state (system init), while it works better when inNet
     * is in a network state.  But this code is also called during network
     * processing, too, so ... we have to push and pop the state or things
     * may get screwy.  At least we lose the routing information (where a
     * message came from) at the moment.
     */
    oldNet = inNet;
    inNet = NORMAL_NET;		/* cheat */
    if ((domain = SearchList(&Serves, RealDomainName(dir->Domain))) != NULL) {
	for (rover = 0; rover < dir->HighFile; rover++) {
	    sprintf(temp, "%d", rover);
	    MakeDomainFileName(buffer, dir->MapDir, temp);
	    if ((netMisc = fopen(buffer, READ_ANY)) != NULL) {
		getMsgStr(getNetChar, nodeId, NAMESIZE);
		getMsgStr(getNetChar, nodeName, NAMESIZE);

		NormStr(nodeId);
		NormStr(nodeName);

		for (c = nodeName; *c; c++)
		    if (*c == '_') *c = ' ';

		/* first check against ourselves (!) */
		if (RouteHere(nodeId, nodeName, domain)) {
		    getRoom(MAILROOM);
		    StartDecode(ReadRoutedDest);
		    RCount = SCount = 0;
		    while (getMessage(ReadRouted, TRUE, TRUE, TRUE)) {
			msgBuf.mbaddr[0] = 0;	/* just in case */
			inRouteMail();
		    }

		    fclose(netMisc);
		    unlink(buffer);
		    dir->UsedFlag = TRUE;
		    continue;
		}
		fclose(netMisc);
		if (AcceptRoute(nodeId, nodeName)) {
		    strCpy(nodeId, netTemp.netId);
		    /* FindRouteSlot(); */
		    /* kludge around a bug (does it exist anymore?) */
		    if (netTemp.nbHiRouteInd < 0) netTemp.nbHiRouteInd = 0;
		    sprintf(temp, "R%d.%d", RouteSlot, netTemp.nbHiRouteInd++);
		    makeSysName(newfn, temp, &cfg.netArea);
		    MoveFile(buffer, newfn);	/* use system dep */
		    if ((netMisc = fopen(newfn, R_W_ANY)) != NULL) {
			fprintf(netMisc, "%-20s", nodeId);
			fclose(netMisc);
		    }
		    netTemp.nbflags.HasRouted = TRUE;
		    putNet(RouteSlot, &netTemp);
		    dir->UsedFlag = TRUE;
		}
	    }
	}
    }
    inNet = oldNet ;
}
