/*
 *				Info.C (try #2)
 *
 * Handles core aspects of the Information command of C-86.
 */

#include "ctdl.h"

/*
 *				history
 *
 * 92May28 HAW	Split into two files.
 * 92May25 HAW	New format created.
 * 89Aug0? HAW	Created.
 */

/*
 *				Contents
 *
 *	ReadCitInfo()		Reads info from file.
 *	WriteOutInformation()	Writes information to disk.
 *	KillInfo()		Kills info of dead room.
 */

extern CONFIG  cfg;

SListBase InfoMap = { NULL, ChkStrForElement, NULL, FreeNtoStr, EatNMapStr };

/*
 * ReadCitInfo()
 *
 * This function will read in the C-86 room info file.
 *
 * Returns TRUE if infomap exists, FALSE otherwise.
 */
char ReadCitInfo()
{
	SYS_FILE fn;
	char read = 0;

	if (read) return TRUE;
	if (!cfg.BoolFlags.NoInfo) {
		makeSysName(fn, "infomap", &cfg.infoArea);
		if (!MakeList(&InfoMap, fn, NULL))
			return FALSE;
	}
	read++;	/* this goes here so if MakeList fails, it doesn't get set */
	return TRUE;
}

/*
 * WriteOutInformation()
 *
 * This function writes out information to disk.
 */
void WriteOutInformation()
{
    SYS_FILE fn;
    extern FILE *upfd;

    makeSysName(fn, "infomap", &cfg.infoArea);
    upfd = fopen(fn, WRITE_TEXT);
    RunList(&InfoMap, WrtNtoStr);
    fclose(upfd);
}

/*
 * KillInfo()
 *
 * This function kills some info.  This is used when a room is killed, so it's
 * legal to kill the room.
 */
void KillInfo(char *name)
{
    NumToString *map;
    char temp[20];
    SYS_FILE sys_name;

    if ((map = SearchList(&InfoMap, name)) != NULL) {
	sprintf(temp, "%d.inf", map->num);
	makeSysName(sys_name, temp, &cfg.infoArea);
	unlink(sys_name);
	KillData(&InfoMap, name);
	WriteOutInformation();
    }
}

