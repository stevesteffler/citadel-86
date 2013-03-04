/*
 *				mailfwd.c
 *
 * Implements mail forwarding.
 */

/*
 *				history
 *
 * 91Sep10 HAW	.ECM support started.
 * 91Feb24 HAW	Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	AddMailForward()	Add forwarding information
 *	KillLocalFwd()		Kills local forwarding of someone
 *	OpenForwarding()	Initializes forwarding code
 *	UpdateForwarding()	Updates the file of addresses
 *
 * Local work:
 *	CheckFwd()		Helps to look for a given person
 *	EatForwarding()		Digests a line from ctdlfwd.sys
 *	FreeForwarding()	Frees a record from the list of addresses
 *	WriteForward()		Writes a record to disk
 */

/*
 * MailForward
 *
 * This contains the list of forwarding address.  If a person is not
 * represented in this list then no forwarding should take place.  This
 * forwarding includes both to other systems and within this system.
 */
void FreeForwarding(), *CheckFwd(), *EatForwarding(char *line);
SListBase MailForward = { NULL, CheckFwd, NULL, FreeForwarding, EatForwarding };
char FindLocal = FALSE;		/* ugly kludge */

extern CONFIG    cfg;		/* Lots an lots of variables    */

/*
 * CheckFwd()
 *
 * This function will check to see if the ForwardMail record under examination
 * represents the indicated person (who parameter), and if the record is for
 * a local or network forward, and match that* information against the state
 * of the global (kludge) variable FindLocal.  It returns NULL unless everything
 * matches, otherwise the address of the forwarding information.  This function
 * is used to search the MailForward list.
 */
static void *CheckFwd(ForwardMail *data, char *who)
{
    if ((!FindLocal && data->System != NULL) ||
    	(FindLocal && data->System == NULL)) {
	return (strCmpU(data->UserName, who) == SAMESTRING) ? data : NULL;
    }
    else
	return NULL;
}

/*
 * CheckFwdAny()
 *
 * This function finds a forwarding record for the specified name.
 */
static void *CheckFwdAny(ForwardMail *data, char *who)
{
    return (strCmpU(data->UserName, who) == SAMESTRING) ? data : NULL;
}

/*
 * EatForwarding()
 *
 * This function eats a line from ctdlfwd.sys.
 *
 * Format: <username><tab><system spec><tab><alias>
 *	where "system spec" can be a domain-name (or nothing at all).
 *	"alias" can be the same as username.
 */
static void *EatForwarding(char *line)
{
    ForwardMail *data;
    char *tab, *tab2;

    if ((tab = strchr(line, '\t')) == NULL) {
	return NULL;
    }
    *tab++ = 0;
    if ((tab2 = strchr(tab, '\t')) == NULL) {
	return NULL;
    }
    *tab2++ = 0;
    data = GetDynamic(sizeof *data);
    data->UserName = strdup(line);
    data->System   = (*tab) ? strdup(tab) : NULL;
    data->Alias    = strdup(tab2);
    CleanEnd(data->Alias);	/* seems ctdlfwd.sys occasionally */
				/* gets trashed -- trailing whitespace */
    return data;
}

/*
 * FreeForwarding()
 *
 * This frees a forwarding record for the MailForward list.
 */
static void FreeForwarding(ForwardMail *data)
{
    free(data->UserName);
    if (data->System != NULL)
	free(data->System);
    free(data->Alias);
    free(data);
}

/*
 * OpenForwarding()
 *
 * This function opens the forwarding data structures.  It should be called
 * only once during Citadel (or utility) initialization.
 */
void OpenForwarding()
{
    SYS_FILE    tempName;

    makeSysName(tempName, "ctdlfwd.sys", &cfg.roomArea);
    MakeList(&MailForward, tempName, NULL);
    if (cfg.weAre == CONFIGUR) {
	MailForward.FreeFunc = NoFree;
	MailForward.CheckIt = CheckFwdAny;
    }
}

/*
 * UpdateForwarding()
 *
 * This function updates ctdlfwd.sys.
 */
void UpdateForwarding()
{
    FILE *MailFwdFd;
    SYS_FILE tempName;
    void WriteForward();
    extern char *WRITE_TEXT;

    makeSysName(tempName, "ctdlfwd.sys", &cfg.roomArea);
    if ((MailFwdFd = fopen(tempName, WRITE_TEXT)) != NULL) {
	RunListA(&MailForward, WriteForward, (void *) MailFwdFd);
	fclose(MailFwdFd);
    }
}

/*
 * WriteForward()
 *
 * This function will write out a record to ctdlfwd.sys.  This is used in
 * conjunction with UpdateForwarding (in particular, a RunList() call).
 */
static void WriteForward(ForwardMail *data, FILE *fd)
{
	/*
	 * this check against NULL is present because we use the same
	 * file for system and non-system forwarding
	 */
    fprintf(fd, "%s\t%s\t%s\n", data->UserName,
			(data->System == NULL) ? "" : data->System,
			data->Alias);
}

/*
 * AddMailForward()
 *
 * This function adds a new account to the forwarding list.  If a record
 * exists for this account, it is deleted first.  The list on disk will
 * also be updated.
 */
void AddMailForward(char *acct, char *system, char *fwdacct)
{
    ForwardMail *address;

    if (system == NULL) FindLocal = TRUE;
    address = GetDynamic(sizeof *address);
    address->UserName = strdup(acct);
    address->System = (system != NULL) ? strdup(system) : NULL;
    address->Alias = strdup(fwdacct);

    /* have to use two steps here rather than just one */
    KillData(&MailForward, acct);
    AddData(&MailForward, address, NULL, FALSE);
    UpdateForwarding();
    FindLocal = FALSE;
}

/*
 * KillLocalFwd()
 *
 * This kills forwarding to a local account -- .ecm.
 */
void KillLocalFwd(char *name)
{
    FindLocal = TRUE;
    KillData(&MailForward, name);
    FindLocal = FALSE;
    UpdateForwarding();
}

/*
 * FindLocalForward()
 *
 * This discovers if the given person has a local forwarding request.  If so,
 * the name of the account is returned.  If not, NULL is returned.
 */
char *FindLocalForward(char *name)
{
    ForwardMail *address;

    FindLocal = TRUE;
    address = SearchList(&MailForward, name);
    FindLocal = FALSE;
    if (address == NULL) return NULL;
    return address->Alias;
}
