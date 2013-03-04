/*
 *				cc.c
 *
 * Other Recipient handling for Citadel-86.
 */

/*
 *				history
 *
 * 89????? HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	DisplayCC()		writes an Other Recipient somewhere.
 *	ShowCC()		show off the list of other recipients.
 */

/************************************************************************/

/************************************************************************/
/*
 * Preliminary comments & code for CC feature in C-86.
 *
 * Comments and notes.
 *
 * o Numerous requests for this feature tend to make it valuable.
 *
 * o Suggest we have a field which may have multiple instances in any message.
 *   This is the 'cc' field.  Suggest further that it be 2 * NAMESIZE bytes
 *   long in order to allow node designators.
 *
 * o Implementation: The getMessage() function should build a linked list
 *   (possibly generic list: libarch.c) as it reads in a message of each
 *   CC recipient.  Will need to have a disposal function for this list as
 *   each message is disposed of.  May take a little code massage to make
 *   sure we hit all the high points.
 *
 * o Only available for Mail>.
 * o (Since getMessage handles all the forms of getting messages from any
 *   source, this handles all the net forms, too.)
 *
 * o Code massage points not otherwise covered in this file:
 *   - Held Messages.
 *   - Interrupted Messages(!).
 *   - getMessage().
 *   - putMessage() - how do we handle the analog to noteMessage()?  New
 *     parameter?
 *
 * o Network provisions
 *   - Net support not present for this type of message.
 *   - New field, net only: Recipient override.  Indicates who mail is to be
 *     given access to this message, may (easily) exclude recipient specified
 *     in the mbto[] field.  New field named mboverride[NAMESIZE * 2].
 *   - Like mbCC, should be handled as list with multiple instances.
 *   - Check for mail should check this list instead of mbto iff mboverride
 *     is present.
 *   - mboverride fields will not contain node designators (initially).
 *     mbCC fields will, where necessary.
 *
 * o Other comments and thoughts about design, etc.
 *   - Unlogged users should not have access.
 *   - List of CC users should be accessible to message writer.
 *   - Able to remove someone from list of CC users???
 *   - Net designation.  Use STadel's '@' convention?  Seems logical.
 *   - NEED FORMAT FOR DISPLAY OF FINISHED & DELIVERED MESSAGE!
 *   - NETHACK3.MAN will need updating.
 *   -
 */

extern MessageBuffer msgBuf;
char CCfirst;

/*
 * ShowCC()
 *
 * This function will show off the current list of other recipients.
 */
void ShowCC(int where)
{
    if (HasCC(&msgBuf)) {
	if (where == SCREEN) 
	    mPrintf("Other recipients:");
	CCfirst = TRUE;
	RunListA(&msgBuf.mbCC, DisplayCC, (void *) where);
	if (where == SCREEN) 
	    doCR();
    }
}

/*
 * DisplayCC()
 *
 * This function writes an Other Recipient somewhere, somehow.
 */
void DisplayCC(char *d, int where)
{
    extern FILE *upfd;

    if (where == SCREEN) {
#ifndef TURBO_C_VSPRINTF_BUG
	mPrintf("%*c%s\n ", (CCfirst) ? 1 : 17, ' ', d);
#else
	SpaceBug((CCfirst) ? 1 : 17); /* sigh */
	mPrintf("%s\n ", d);
#endif
    }
    else if (where == MSGBASE)
	dPrintf("W%s", d);
    else if (where == TEXTFILE)
	fprintf(upfd, "%s\n", d);
    CCfirst = FALSE;
}
