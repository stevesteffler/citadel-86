/*
 *				formhdr.c
 *
 * formHeader() call
 */

/*
 *				history
 *
 * 96Sep12 HAW  Created so we can link to msgadd.
 */

#define MSG_INTERNALS
#include "ctdl.h"

/*
 *				contents
 *
 *      formHeader()            format message header for user
 */

extern MessageBuffer     msgBuf;
extern aRoom	         roomBuf;		/* Room buffer		*/
extern CONFIG	         cfg;		/* Lots an lots of variables    */
extern int	         thisRoom;
extern char              *R_SH_MARK;
extern char              *LOC_NET, *NON_LOC_NET, *ALL_LOCALS;
extern rTable	         *roomTab;
extern char              outFlag;
extern logBuffer         logBuf;
extern char	         prevChar;

char        EOP = FALSE;
AN_UNSIGNED crtColumn; /* current position on screen		*/

/*
 * formHeader()
 *
 * This returns a string with the msg header formatted.
 */
char *formHeader(char showtime)
{
    static char header[250];

    header[0] = 0;			/* Initialize the genie.... */

    if (msgBuf.mbdate[ 0])  sprintf(lbyte(header), "   %s ", msgBuf.mbdate);
    if (msgBuf.mbtime[ 0] && showtime)
				sprintf(lbyte(header), "%s ", msgBuf.mbtime);
    if (msgBuf.mbauth[ 0]) {
	sprintf(lbyte(header), "from %s",    msgBuf.mbauth );
    }
    NormStr(msgBuf.mboname);
    if (msgBuf.mboname[0]) {
	sprintf(lbyte(header), " @ %s", msgBuf.mboname);
	if (msgBuf.mbdomain[0])
	    sprintf(lbyte(header), cfg.DomainDisplay, msgBuf.mbdomain);
    }

    if (strCmpU(msgBuf.mbroom, roomBuf.rbname) != SAMESTRING) {
	strcat(header, " in ");
	if (roomExists(msgBuf.mbroom) != ERROR)
	    sprintf(lbyte(header), formRoom(roomExists(msgBuf.mbroom), FALSE,
								FALSE));
	else
	    sprintf(lbyte(header), "%s>", msgBuf.mbroom);
    }

    if (msgBuf.mbto[   0]) {
	sprintf(lbyte(header), " to %s", msgBuf.mbto);
	if (!msgBuf.mbauth[0] && thisRoom == MAILROOM &&
	    strLen(cfg.SysopName) != 0)			/* Mail to sysop */
	    sprintf(lbyte(header), " (%s)", cfg.SysopName);
    }

    if (msgBuf.mbaddr[ 0] &&
     strncmp(msgBuf.mbaddr, R_SH_MARK, strLen(R_SH_MARK)) != SAMESTRING &&
     strncmp(msgBuf.mbaddr, LOC_NET, strLen(LOC_NET)) != SAMESTRING &&
     strncmp(msgBuf.mbaddr, NON_LOC_NET, strLen(NON_LOC_NET)) != SAMESTRING)
	sprintf(lbyte(header), " (on %s)", strCmpU(msgBuf.mbaddr, ALL_LOCALS) ?
					msgBuf.mbaddr : "All Local Systems");

    if (msgBuf.mbMsgStat[0])
	sprintf(lbyte(header), " (%s)", msgBuf.mbMsgStat);

    return header;
}

char	*APrivateRoom = "A Private Room";

/*
 * formRoom()
 *
 * This returns a string with the room formatted, including the prompt type.
 */
char *formRoom(int roomNo, int showPriv, int noDiscrimination)
{
    static char display[40];
    int		one, two;
    static char matrix[2][2] =
	{  { '>', ')' } ,
	{ ']', ':' } } ;

    one = roomTab[roomNo].rtflags.ISDIR;
    two = (roomTab[roomNo].rtflags.SHARED && cfg.BoolFlags.netParticipant);
    if (roomTab[roomNo].rtflags.INUSE) {
	if (!noDiscrimination &&
	    !roomTab[roomNo].rtflags.PUBLIC)
	    strcpy(display, APrivateRoom);
	else {
	    sprintf(display, "%s%c%s",
		roomTab[roomNo].rtname,
		matrix[one][two],
		(!roomTab[roomNo].rtflags.PUBLIC && showPriv) ? "*" : "");
	}
    }
    else display[0] = '\0';
    return display;
}

/*
 * roomExists()
 *
 * This returns slot# of named room else ERROR.
 */
int roomExists(char *room)
{
    int i;

    for (i = 0;  i < MAXROOMS;  i++) {
	if (
	    roomTab[i].rtflags.INUSE == 1   &&
	    strCmpU(room, roomTab[i].rtname) == SAMESTRING
	) {
	    return(i);
	}
    }
    return(ERROR);
}

/*
 * mFormat()
 *
 * This function does the work of formatting a string to modem and console.
 */
void mFormat(char *string, void (*out)(char), char (*eol)(void))
{
    char wordBuf[MAXWORD];
    int  i;

    for (i = 0;  string[i] && (outFlag == OUTOK	 ||
			     outFlag == IMPERVIOUS ||
			     outFlag == NO_CANCEL ||
			     outFlag == OUTPARAGRAPH);  ) {
	i = getWord(wordBuf, string, i, MAXWORD);
	if (!putWord(wordBuf, out, eol)) return;
	if (mAbort()) return;
    }
}

/*
 * getWord()
 *
 * This function fetches one word from current message.
 */
int getWord(char *dest, char *source, int offset, int lim)
{
    int i, j;

    /* skip leading blanks if any */
    for (i = 0; (source[offset+i] == '\n' || source[offset+i] == ' ') &&
							i < lim - 1;  i++);

    /* step over word */
    for (;

	 source[offset+i]   != ' '     &&
	 source[offset+i]   != '\n'     &&
	 i		  <  lim - 1 &&
	 source[offset+i]   != 0;

	 i++
    );

    if (source[offset + i - 1] != '\n')
	/* pick up any trailing blanks */
	for (;  source[offset+i]==' ' && i<lim - 1;  i++);

    /* copy word over */
    for (j = 0; j < i; j++)  dest[j] = source[offset+j];
    dest[j] = 0;	/* null to tie off string */

    return(offset+i);
}

/*
 * putWord()
 *
 * This function writes one word to modem & console.
 */
char putWord(char *st, void (*out)(char), char (*eol)(void))
{
    char *s;
    int  newColumn;

    for (newColumn = crtColumn, s = st;  *s; s++)   {
	if (*s != TAB) {
	    if (*s == '\b') newColumn--;
	    else if (*s == '\n') {
		if (*(s+1) == '\n' || *(s+1) == ' ')
		    newColumn = 1;
		else ++newColumn;
	    }
	    else ++newColumn;
	}
	else	    while (++newColumn % 8);
    }
    if (newColumn > termWidth) {
	if (!(*eol)()) {
		return FALSE;
	}
	if (*st == '\n' && *(st+1) != '\n' && *(st+1) != ' ' && *(st+1))
	    st++;
    }

    BufferingOn();
    
    for (;  *st;  st++) {

#ifdef OLD_STYLE
	if (*st != TAB) ++crtColumn;
	else	    while (++crtColumn % 8);
#else
#ifdef NEEDED
	if (*st != TAB) {
	    if (*st == '\b') crtColumn--;
	    else ++crtColumn;
	}
	else	    while (++crtColumn % 8);
#endif
#endif

	/* worry about words longer than a line:	*/
	if (crtColumn > termWidth) {
	    if (!(*eol)()) {
		return FALSE;
	    }
	}

	if (*st == '\n' && EOP) {
	    if (!(*eol)()) {
		return FALSE;
	    }
	}
	else if (prevChar!=NEWLINE  ||  (*st > ' ')) {
	    (*out)(*st);
	    if (*st > ' ') EOP = FALSE;
	}
	else {
	    /* end of paragraph: */
	    if (outFlag == OUTPARAGRAPH)   {
		outFlag = OUTOK;
	    }
	    if (!(*eol)()) {
		return FALSE;
	    }
	    if (*st == '\n' && !EOP) {
		if (!(*eol)()) {
			return FALSE;
	        }
	    }
	    else (*out)(*st);
	    EOP = TRUE;
	}
    }
    BufferingOff();
    return TRUE;
}
