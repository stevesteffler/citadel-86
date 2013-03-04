
/*
 *				Mail.c
 * Miscellaneous mail functions.
 */

#include "ctdl.h"

/*
 *	                        history
 *
 * 97May03 HAW  Completed initial version.
 */

/*
 *	                        Contents
 *
 * IgnoreThisUser		Stores a request to ignore a user's mail
 */

/*
 * NOTE:
 *
 * This includes an implementation of a data file for handling the problem
 * of a user not wanting to accept mail from someone else.
 */

typedef struct {
	int auth_slot;
	int detested_slot;
} IgnoreUserMailRecord;

extern FILE    *upfd;
extern CONFIG  cfg;
extern int     thisLog;

static SYS_FILE IgnoreMailFileName;
static char IgnoreMailDisturbed;

void InitIgnoreMail()
{
	makeSysName(IgnoreMailFileName, "igmail.sys", &cfg.roomArea);
}

/*
 * IgnoreThisUser
 *
 * This function is called when a user has been selected to be ignored.
 * The current user is considered to be authorizing the ignorance.
 */
int IgnoreThisUser(int slot)
{
	IgnoreUserMailRecord mr;

	mr.auth_slot     = thisLog;
	mr.detested_slot = slot;

	if ((upfd = fopen(IgnoreMailFileName, APPEND_ANY)) == NULL) {
		printf("Failure to open %s, errno %d.\n",
			IgnoreMailFileName,
			errno);
		return FALSE;
	}

	if (fwrite(&mr, 1, sizeof mr, upfd) != sizeof mr) {
		printf("Failure to write to %s, errno %d.\n",
			IgnoreMailFileName,
			errno);
		fclose(upfd);
		return FALSE;
	}

	fclose(upfd);
	return TRUE;
}

/*
 * AcceptableMail
 *
 * Is the mail from the current user acceptable to slot?
 */
int AcceptableMail(int from, int target)
{
	IgnoreUserMailRecord mr;
	char found = FALSE;
	int slot = 0;

	if ((upfd = fopen(IgnoreMailFileName, READ_ANY)) == NULL) {
		return TRUE;
	}

	while (!found && fread(&mr, 1, sizeof mr, upfd) == sizeof mr) {
		if (target == mr.auth_slot && from == mr.detested_slot) {
			found = TRUE;
		}
		if (-1 == mr.auth_slot && -1 == mr.detested_slot) {
			printf("Slot %d empty.\n", slot);
		}
		slot++;
	}

	fclose(upfd);
	return !found;
}

/*
 * IgnoredUsers()
 *
 * This function traverses the list of ignored users for some user, calling
 * the specified callback function for each.
 */
int IgnoredUsers(int from, int (*fn)(int))
{
	IgnoreUserMailRecord mr;

	if ((upfd = fopen(IgnoreMailFileName, READ_ANY)) == NULL) {
		return TRUE;
	}

	while (fread(&mr, 1, sizeof mr, upfd) == sizeof mr) {
		if (from == mr.auth_slot) {
			if (!(*fn)(mr.detested_slot))
				break;
		}
	}

	fclose(upfd);

	return TRUE;
}

/*
 * IgMailRemoveEntries
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

static void FreeIgMail();
/*
 * IgMailCleanup()
 *
 * Cleans up the ignore mail file.
 */
void IgMailCleanup()
{
	IgnoreUserMailRecord mr, *rec;
	static SListBase records = { NULL, NULL, NULL, FreeIgMail, NULL };

	if (IgnoreMailDisturbed) {
		IgnoreMailDisturbed = FALSE;

		if ((upfd = fopen(IgnoreMailFileName, READ_ANY)) == NULL) {
			return;
		}

		while (fread(&mr, 1, sizeof mr, upfd) == sizeof mr) {
			if (-1 != mr.auth_slot && -1 != mr.detested_slot) {
				rec = GetDynamic(sizeof *rec);
				*rec = mr;
				AddData(&records, rec, NULL, FALSE);
			}
		}

		fclose(upfd);
		unlink(IgnoreMailFileName);

		if ((upfd = fopen(IgnoreMailFileName, WRITE_ANY)) == NULL) {
			return;
		}

		KillList(&records);

		fclose(upfd);
	}
}

static void FreeIgMail(IgnoreUserMailRecord *rec)
{
	fwrite(rec, 1, sizeof *rec, upfd);
	free(rec);
}

