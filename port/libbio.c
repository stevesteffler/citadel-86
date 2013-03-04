
/*
 *				libbio.c
 *
 * User Biography Functions for use by utilities.
 */

/*
 *				history
 *
 * 96Sep02 HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	ClearBio()		This clears a bio from disk.
 */

extern CONFIG      cfg;		/* Lots an lots of variables    */

/*
 * ClearBio()
 *
 * This clears out a biography.
 */
void ClearBio(int which)
{
    char     name[15];
    SYS_FILE bio;

    if (!cfg.BoolFlags.NoMeet) {
	sprintf(name, "%d.bio", which);
	makeSysName(bio, name, &cfg.bioArea);
	unlink(bio);
    }
}
