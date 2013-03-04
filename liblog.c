/*
 *				liblog.c
 *
 * Citadel log code for the library.
 */

/*
 *				history
 *
 * 85Nov15 HAW  File created.
 */

#include "ctdl.h"
#include "ctdlansi.h"
/*
 *				contents
 *
 *	getLog()		loads requested CTDLLOG record
 *	putLog()		stores a logBuffer into citadel.log
 */

logBuffer logBuf;		/* Log buffer of a person       */
logBuffer logTmp;		/* Useful global buffer		*/
int       thisLog;		/* entry currently in logBuf    */
FILE      *logfl;		/* log file descriptor		*/
extern CONFIG cfg;		/* Configuration variables      */

extern FILE *upfd;
static SYS_FILE IgnoreMailFileName;
static char IgnoreMailDisturbed;

/* copied over from MAIL.C */
typedef struct {
        int auth_slot;
        int detested_slot;
} IgnoreUserMailRecord;

/*
 * IgMailRemoveEntries - snarfed over from MAIL.C
 *
 * This function removes the specified author/target pair from the list of
 * ignored users.  If target is -1, all entries pertaining to source are
 * removed (for instance, if a user is deleted or rolled over).  Same if
 * source is -1.
 */
int IgMailRemoveEntries(int source, int target)
{
        IgnoreUserMailRecord mr;
        int tracker;

        if ((upfd = fopen(IgnoreMailFileName, R_W_ANY)) == NULL) {
                return TRUE;
        }

        for (tracker = 0;
             fread(&mr, 1, sizeof mr, upfd) == sizeof mr;
             tracker++) {
                if ((mr.auth_slot == source && mr.detested_slot == target) ||
                    (source == -1 && mr.detested_slot == target) ||
                    (mr.auth_slot == source && target == -1)) {
                        mr.auth_slot = -1;
                        mr.detested_slot = -1;
                        fseek(upfd, tracker * sizeof mr, 0);
                        fwrite(&mr, 1, sizeof mr, upfd);
                        IgnoreMailDisturbed = TRUE;
                }
        }

        fclose(upfd);

        return TRUE;
}


/*
 * getLog()
 *
 * This loads requested log record into the specified RAM buffer.
 */
void getLog(logBuffer *lBuf, int n)
{
    long int s, r;

    if (lBuf == &logBuf)   thisLog      = n;

    r = LB_TOTAL_SIZE;		/* To get away from overflows   */
    s = n * r;			/* This should be offset	*/
    n *= 3;

    fseek(logfl, s, 0);

    if (fread(lBuf, LB_SIZE, 1, logfl) != 1) {
	crashout("?getLog-read fail//EOF detected (1)!");
    }

    crypte(lBuf, LB_SIZE, n);	/* decode buffer    */

    if (fread(lBuf->lbrgen, GEN_BULK, 1, logfl) != 1) {
	crashout("?getLog-read fail//EOF detected (2)!");
    }

    if (fread(lBuf->lbMail, MAIL_BULK, 1, logfl) != 1) {
	crashout("?getLog-read fail//EOF detected (3)!");
    }

    if (fread(lBuf->lastvisit, RM_BULK, 1, logfl) != 1) {
	crashout("?getLog-read fail//EOF detected (4)!");
    }
}

/*
 * putLog()
 *
 * This function stores the given log record into ctdllog.sys.
 */
void putLog(logBuffer *lBuf, int n)
{
    long int s, r;

    r = LB_TOTAL_SIZE;
    s = n * r;
    n   *= 3;

    crypte(lBuf, LB_SIZE, n);		/* encode buffer	*/

    fseek(logfl, s, 0);

    if (fwrite(lBuf, LB_SIZE, 1, logfl) != 1) {
	crashout("?putLog-write fail (1)!");
    }

    if (fwrite(lBuf->lbrgen, GEN_BULK, 1, logfl) != 1) {
	crashout("?putLog-write fail (2)!");
    }

    if (fwrite(lBuf->lbMail, MAIL_BULK, 1, logfl) != 1) {
	crashout("?putLog-write fail (3)!");
    }

    if (fwrite(lBuf->lastvisit, RM_BULK, 1, logfl) != 1) {
	crashout("?putLog-write fail (4)!");
    }

    crypte(lBuf, LB_SIZE, n);	/* encode buffer	*/

    fflush(logfl);
}

char	*LCHeld = "log%d.hld";
/*
 * RemoveUser()
 *
 * This function handles removing a user from the log.
 */
void RemoveUser(int logNo, logBuffer *lBuf)
{
	extern SListBase Moderators;
	extern SListBase MailForward;
	SYS_FILE killHeld;
	void *RemoveModerator();
	char heldbuf[20];

	/* remove old held message */

	if (cfg.BoolFlags.HoldOnLost) {
		sprintf(heldbuf, LCHeld, logNo);
		makeSysName(killHeld, heldbuf, &cfg.holdArea);
		unlink(killHeld);
	}

	/* remove mail forwarding */

	KillData(&MailForward, lBuf->lbname);
	UpdateForwarding();

	/* remove moderator listing */
	AltKillData(&Moderators, RemoveModerator, lBuf->lbname);
	WriteAList(&Moderators, "ctdlmodr.sys", WrtNtoStr);

	/* remove bio */
	ClearBio(logNo);

	/* clear ignore mail stuff */
	IgMailRemoveEntries(logNo, -1);
	IgMailRemoveEntries(-1, logNo);
}


static void *RemoveModerator(NumToString *element, char *name)
{
	if (strCmpU(element->string, name) == 0)
		return element;
	return NULL;
}

