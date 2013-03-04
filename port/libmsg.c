/*
 *				libmsg.c
 *
 * Common message handling functions for Citadel bulletin board system.
 */

/*
 *				history
 *
 * 88Oct12 HAW  Generalize getMessage()
 * 87Feb11 HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	getMessage()		load message into RAM
 *	getMsgStr()		reads a string out of message.buf
 *	getMsgChar()		returns successive chars off disk
 *	startAt()		setup to read a message off disk
 *	unGetMsgChar()		return a char to getMsgChar()
 */

struct mBuf	mFile1, mFile2;
MessageBuffer	msgBuf, tempMess;
FILE		*msgfl, *msgfl2;

static int  GMCCache;

extern CONFIG cfg;
char	*R_SH_MARK   = "&&";
char	*LOC_NET     = "++";
char	*NON_LOC_NET = "%%";

/*
 * getMessage()
 *
 * This function reads a message off disk into RAM.  The source of the message
 * is controlled by the Source parameter; if the source is ctdlmsg.sys, then
 * a previous call to setUp has specified the message.
 *
 * FromNet - this indicates if the message is coming from a network-generated
 * temporary file (TRUE) or from ctdlmsg.sys (FALSE).  If the latter then this
 * function must find the start of message within the current sector.  Also,
 * the 'w' field of messages has different meanings depending on the source of
 * the message (messy, but useful).
 *
 * all - TRUE indicates the message text should be read in, FALSE indicates
 * reading should end when the 'M' field is encountered.
 *
 * ClearOthers - indicates all lists associated with msgBuf should be cleared
 * before the message is read in.
 */
char getMessage(int (*Source)(void), char FromNet, char all, char ClearOthers)
{
	int  c;
	char CC[CC_SIZE], *d;

	/* clear msgBuf out */
	if (ClearOthers)
		ZeroMsgBuffer(&msgBuf);

	if (!FromNet) {      /* only do this for messages from our own msgs */
		do {
			c = getMsgChar();
		} while (c != 0xFF);     /* find start of msg    */

		msgBuf.mbheadChar   = mFile1.oldChar;	/* record location      */
		msgBuf.mbheadSector = mFile1.oldSector;

		getMsgStr(getMsgChar, msgBuf.mbId, NAMESIZE);
	}

	do  {
		c = (*Source)();
		switch (c) {
		case 'A':
			getMsgStr(Source, msgBuf.mbauth, sizeof msgBuf.mbauth);
			break;
		/* MsgAdd / MsgOut special */
		case 'F':
			getMsgStr(Source, msgBuf.mbFileName, sizeof
				msgBuf.mbFileName);
			break;
		case 'D':
			getMsgStr(Source, msgBuf.mbdate,  NAMESIZE);
			break;
		case 'C':
			getMsgStr(Source, msgBuf.mbtime,  NAMESIZE);
			break;
		case 'H':
			getMsgStr(Source, msgBuf.mbMsgStat,  NAMESIZE);
			break;
		case 'M': /* just exit -- we'll read off disk */
			break;
		case 'N':
			getMsgStr(Source, msgBuf.mboname, NAMESIZE);
			while ((d = strchr(msgBuf.mboname, '_')) != NULL)
				*d = ' ';
			break;
		case 'O':
			getMsgStr(Source, msgBuf.mborig,  NAMESIZE);
			break;
		case 'R':
			getMsgStr(Source, msgBuf.mbroom,  NAMESIZE);
			break;
		case 'S':
			getMsgStr(Source, msgBuf.mbsrcId, NAMESIZE);
			break;
		case 'T':
			getMsgStr(Source, msgBuf.mbto, sizeof msgBuf.mbto);
			break;
		case 'Q':
			getMsgStr(Source, msgBuf.mbaddr,  sizeof msgBuf.mbaddr);
			break;
		case 'X':
			getMsgStr(Source, msgBuf.mbdomain,NAMESIZE);
			break;
		case 'P':
			getMsgStr(Source, msgBuf.mbOther, O_NET_PATH_SIZE); 
			break;
		case 'W':
			getMsgStr(Source, CC, CC_SIZE);
			if (ClearOthers)
				AddData(&msgBuf.mbCC, strdup(CC), NULL, FALSE);
			break;
		case 'w':
			if (FromNet) {
				getMsgStr(Source, CC, CC_SIZE);
				if (ClearOthers)
					AddData(&msgBuf.mbOverride,
						strdup(CC), NULL, FALSE);
		 	}
			else { /* now we can overload this -- 'w' is net only */
				getMsgStr(Source, msgBuf.mbreply, NAMESIZE);
			}
			break;
			/* yes, this is correct - we're overloading mbaddr. */
		case 't':
			getMsgStr(Source, msgBuf.mbaddr, sizeof msgBuf.mbaddr);
			break;
		case -1 : return FALSE;
		default:
			if (isprint(c)) {
			/* save foreign fields */
				msgBuf.mbtext[0] = c;
				getMsgStr(Source, msgBuf.mbtext+1,MAXTEXT- 5);
				AddData(&msgBuf.mbForeign,strdup(msgBuf.mbtext),
								NULL, FALSE);
				msgBuf.mbtext[0]    = '\0';
			}
			else if (c == 0xFF && !FromNet) {	/* Damaged msgBase   */
				unGetMsgChar(c);
			}
			break;
		}
	} while (c != 'M'  &&  isprint(c));

	if (c == 'M') {
		if (all) getMsgStr(Source, msgBuf.mbtext, MAXTEXT);
		return TRUE;
	}
	return FALSE;
}

/*
 * getMsgStr()
 *
 * This function reads a string from the given source.
 */
void getMsgStr(int (*Source)(void), char *dest, int lim)
{
    int  c;

    while ((c = (*Source)()) && c != EOF) {
					/* read the complete string     */
	if (lim) {			/* if we have room then		*/
	    lim--;
	    if (c == '\r') c = '\n';
	    *dest++ = c;		/* copy char to buffer		*/
	}
    }
    if (!lim) dest--;   /* Ensure not overwrite next door neighbor      */
    *dest = '\0';			/* tie string off with null     */
}

/*
 * getMsgChar()
 *
 * This function returns sequential chars from CtdlMsg.Sys.
 */
int getMsgChar()
{
    long work;
    int  toReturn;

    if (GMCCache) {     /* someone did an unGetMsgChar() --return it    */
	toReturn= GMCCache;
	GMCCache= '\0';
	return (toReturn & 0xFF);
    }

    mFile1.oldChar     = mFile1.thisChar;
    mFile1.oldSector   = mFile1.thisSector;

    toReturn = mFile1.sectBuf[mFile1.thisChar];
    toReturn &= 0xFF;   /* Only want the lower 8 bits */

    mFile1.thisChar    = ++mFile1.thisChar % MSG_SECT_SIZE;
    if (mFile1.thisChar == 0) {
	/* time to read next sector in: */
	mFile1.thisSector  = ++mFile1.thisSector % cfg.maxMSector;
	work = mFile1.thisSector;
	work *= MSG_SECT_SIZE;
	fseek(msgfl, work, 0);
	if (fread(mFile1.sectBuf, MSG_SECT_SIZE, 1, msgfl) != 1) {
	    crashout("?nextMsgChar-read fail");
	}
	crypte(mFile1.sectBuf, MSG_SECT_SIZE, 0);
    }
    return(toReturn);
}

/*
 * startAt()
 *
 * This function sets the location to begin reading a message from. This is
 * usually only the sector, since byte offset within the sector is not
 * saved.
 */
void startAt(FILE *whichmsg, struct mBuf *mFile, SECTOR_ID sect, int byt)
{
    long temp;

    GMCCache  = '\0';   /* cache to unGetMsgChar() into */

    if (sect >= cfg.maxMSector) {
	printf("?startAt s=%u,b=%d", sect, byt);
	return ;   /* Don't crash anymore, just skip the msg */
    }
    mFile->thisChar    = byt;
    mFile->thisSector  = sect;

    temp = sect;
    temp *= MSG_SECT_SIZE;
    fseek(whichmsg, temp, 0);
    if (fread(mFile->sectBuf, MSG_SECT_SIZE, 1, whichmsg) != 1) {
	crashout("?startAt read fail");
    }
    crypte(mFile->sectBuf, MSG_SECT_SIZE, 0);
}

/*
 * unGetMsgChar()
 *
 * This function returns (at most one) char to getMsgChar().
 */
void unGetMsgChar(char c)
{
    GMCCache    = (int) c;
}

/*
 * ZeroMsgBuffer()
 *
 * This function zeroes a message buffer.  This includes clearing lists out
 * legally, etc.
 */
void ZeroMsgBuffer(MessageBuffer *msg)
{
    memset(msg, 0, STATIC_MSG_SIZE);
    KillList(&msg->mbCC);
    KillList(&msg->mbOverride);
    KillList(&msg->mbForeign);
    KillList(&msg->mbInternal);
    msg->mbtext[0] = 0;
}

/*
 * InitMsgBase()
 *
 * This function opens the msg base(s), inits the msg buffers.
 */
void InitMsgBase()
{
    SYS_FILE name;

    makeSysName(name, "ctdlmsg.sys", &cfg.msgArea);
    openFile(name, &msgfl);

    if (cfg.BoolFlags.mirror) {
	makeSysName(name, "ctdlmsg.sys", &cfg.msg2Area);
	openFile(name, &msgfl2);
    }
    InitBuffers();
}

/*
 * InitBuffers()
 *
 * This function initializes the message buffers.
 */
void InitBuffers()
{
    static SListBase MsgBase = { NULL, ChkCC, NULL, free, strdup };

    copy_struct(MsgBase, msgBuf.mbCC);
    copy_struct(MsgBase, msgBuf.mbOverride);
    copy_struct(MsgBase, msgBuf.mbInternal);
    copy_struct(MsgBase, msgBuf.mbForeign);

    copy_struct(MsgBase, tempMess.mbOverride);
    copy_struct(MsgBase, tempMess.mbCC);
    copy_struct(MsgBase, tempMess.mbInternal);
    copy_struct(MsgBase, tempMess.mbForeign);

    msgBuf.mbtext   = GetDynamic(MAXTEXT);
    tempMess.mbtext = GetDynamic(MAXTEXT);
}

/*
 * findMessage()
 *
 * This function will get all set up to do something with a message.
 */
char findMessage(SECTOR_ID loc, MSG_NUMBER id, char ClearOthers)
{
    MSG_NUMBER here;

    id &= S_MSG_MASK;
    startAt(msgfl, &mFile1, loc, 0);

    do {
	getMessage(getMsgChar, FALSE, FALSE, ClearOthers);
	here = atol(msgBuf.mbId);
    } while (here != id &&  mFile1.thisSector == loc);

    if (here != id) mPrintf("Ooops, couldn't find %ld @ %d.\n ", id, loc);
    return ((here == id));
}

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
 *
 * ALSO SEE CC.C.
 */

/*
 * ChkCC()
 *
 * This will see if the two records are identical.  This is used for list
 * searches.
 */
void *ChkCC(char *d1, char *d2)
{
    return (strCmpU(d1, d2) == SAMESTRING) ? d1 : NULL;
}
