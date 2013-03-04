/*
 *				Dom-init.C
 *
 * Domain handing code.
 */

#define NET_INTERNALS
#define NET_INTERFACE

#include "ctdl.h"

/*
 *				history
 *
 * 93Jun10 HAW  Created.
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
extern SListBase DomainMap;

extern char *DomainFlags /*     = NULL */;

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
 * UtilDomainInit()
 *
 * This function initializes the domain stuff.  It's called from NetInit(),
 * addNetNode(), and from RationalizeDomains, so it has to open the relevant
 * files only once.
 *
 * Appropriate lists are created, call out flags set, etc.
 */
void UtilDomainInit(char FirstTime)
{
    SYS_FILE temp;

    if (FirstTime) {
	makeSysName(temp, MAPSYS, &cfg.domainArea);
	MakeList(&DomainMap, temp, NULL);
    }
    else
	if (DomainFlags != NULL) free(DomainFlags);

    if (cfg.netSize != 0)
	DomainFlags = GetDynamic(cfg.netSize);	/* auto-zeroed */

    /* This will also set the temporary call out flags, eh?		*/
}

/*
 * UpdateMap()
 *
 * This function is charged with updating MAP.SYS with new information.
 * Most of the work is done in WriteDomainMap().
 */
void UpdateMap()
{
    SYS_FILE temp;
    void WriteDomainMap();
    FILE *fd;

    makeSysName(temp, MAPSYS, &cfg.domainArea);
    if ((fd = fopen(temp, WRITE_TEXT)) != NULL) {
	RunListA(&DomainMap, WriteDomainMap, fd);
	fclose(fd);
    }
}

/*
 * WriteDomainMap()
 *
 * This writes out an entry of the domain map.
 */
static void WriteDomainMap(DomainDir *data, FILE *fd)
{
	if (data->HighFile > 0)
		fprintf(fd, "%d %s %d\n", data->MapDir, data->Domain,
							data->HighFile);
}

typedef struct {
	char (*fn)();
	int count;
	char lc;
} DO_Args;

int UtilDomainOut(char (*f)(char *name, char *domain, char LocalCheck),
						char LocalCheck)
{
	DO_Args args;
	void SendDomainMail();

	args.count = 0;
	args.fn = f;
	args.lc = LocalCheck;
	RunListA(&DomainMap, SendDomainMail, &args);
	return args.count;
}

/*
 * SendDomainMail()
 *
 * This does the actual work of sending the mail.  It cycles through all the
 * mail in the given domain's directory, sending each piece in sequence.
 * Rejected mail, if any, is kept for another try later, and later processing
 * will close up any "holes" left in the numerical naming sequence for mail
 * files.  That same later processing will also kill empty directories.
 */
void SendDomainMail(DomainDir *data, DO_Args *args)
{
    DOMAIN_FILE buffer;
    int rover, result;
    label temp;

    /* This domain is to be delivered to current system? */
    if (data->TargetSlot == thisNet) {
	data->UsedFlag = TRUE;
	for (rover = 0; rover < data->HighFile; rover++) {
	    sprintf(temp, "%d", rover);
	    MakeDomainFileName(buffer, data->MapDir, temp);
	    if ((result=(*args->fn)(buffer, data->Domain, args->lc))
							== GOOD_SEND) {
		unlink(buffer);
		args->count++;
	    }
	    else if (result == REFUSED_ROUTE) {
	    }
	    else if (result == STOP_TRAVERSAL) {
		return;
	    }
	}
    }
}

