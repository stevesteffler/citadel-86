/* Notes:
   In CTDL.C instead of have the doHelp() call tutorial() it should now
   call printHelp(filename).
   The code isn't commented, but you should be able to see what's happening,
   it is very straigtforward. If you can't decipher something drop me a
   note.

   In the HeLP files, you insert lines containing % sign followed by the
   topic (read: filename) of the entries you want to have displayed in
   the menu. Add a space and then enter text to describe it.
   For example, an exceprt from a help file:

   %FILES This menu item will display FILES.HLP
   %DOHELP This entry will re-show the main help file
   %FOO Help for idiots... :-)

   etc...

   The file nsame will be padded out to 8 characters and a letter inside
   square brackets will be added. The above will format into:

   [a] FILES     This menu item will display FILES.HLP
   [b] DOHELP    This entry will reshow the main help file
   [c] FOO       Help for idiots... :-)

   And then the prompt asking for a choice will appear. Every help file
   can contain these entries, and there is no limit to the depth that
   this routine can display menus.  If there are no % signs in the help
   file then no prompt for a choice is printed (cause no choices were
   displayed, right?).  I believe these are all the changes I have made,
   I put these routines in MISC.C...

		Paul Gauthier
   */

#include "ctdl.h"

extern CONFIG    cfg;
extern logBuffer logBuf;
extern char      *NoFileStr;
extern char      outFlag;
extern char      haveCarrier;
extern char      onConsole;
extern char	 whichIO;
extern int	 CurLine;

/*
 * commands 118.662
 *
 * Problem: in a long help file, with paging, hitting '?'.  Results in help
 * file code being called from help file code.  Unfortunately, the code Paul
 * wrote is not re-entrant.  This structure allows us to go re-entrant.
 *
 * Yeah, I'd rather rewrite it, but it's really not worth the time.
 */
typedef struct {
	FILE *fd;
	int  aloop, PB;
	char line[MAXWORD];
} HELP_FILE;

char getFileChar(HELP_FILE *file, int init);
int  getHelpNames(HELP_FILE *file, char *name, char count, int flags);
/************************************************************************/
/*    printHelp() does a tree structured help tutorial			*/
/************************************************************************/
int printHelp(char *filename, int flags)
{
HELP_FILE file = { NULL, -1, 0, "" };
char nextfile[NAMESIZE + 10];
char next= TRUE;
char list[26][NAMESIZE + 10];
char nlist= 0;
int toReturn, errcount;
SYS_FILE fn;
extern char *READ_TEXT;
char hit, init;

if (!Pageable) init = TRUE;
else init = FALSE;
if (!(flags & HELP_NOT_PAGEABLE)) PagingOn();
toReturn= TRUE;
strcpy(nextfile, filename);

while (next && toReturn != FALSE)
    {
    nlist= 0;
    if (flags & HELP_LEAVE_ALONE)
	strcpy(fn, nextfile);
    else if (flags & HELP_USE_BANNERS)
	makeBanner(fn, nextfile);
    else
	makeSysName(fn, nextfile, &cfg.homeArea);
    if ((file.fd = safeopen(fn, READ_TEXT)) == NULL)
	    {
	    if (flags & HELP_BITCH) mPrintf(NoFileStr, nextfile);
	    toReturn= FALSE;
	    }
    else
	    {
	    if (init) CurLine = 1;
	    getFileChar(&file, 1);
	    if (outFlag != NO_CANCEL) outFlag = OUTOK;
	    if (!expert && !(flags & HELP_NO_HELP)) {
		if (flags & HELP_SHORT) mPrintf("\n <J>ump <P>ause <S>top\n");
		else
		    mPrintf("\n <J>ump to skip to the next paragraph\n <P>ause to pause the text\n <S>top to exit the help\n");
	    }
	    mPrintf(" \n");
	    while (getHelpNames(&file, list[nlist], nlist, flags) && nlist < 25)
		   nlist++;
		   nlist--;
		   if (nlist == -1)
		   {
			next= 0;
		   }
		else
		   {
		    outFlag = OUTOK;
		    mPrintf("\n Hit RETURN to exit help, or");
		    if (nlist == 0)
			mPrintf(" press the letter of your choice: ");
		    else
			mPrintf(" press the letter of your choice [a-%c]: ", 'a' + nlist);
		    next = errcount = 0;
		    while (onLine() && !next && (hit= toUpper(modIn())) != '\r' && hit != '\n')
			  {
			  if (hit - 'A' < 0 || hit - 'A' > nlist) {
				if (++errcount > 5 && whichIO == MODEM)
					HangUp(FALSE);
			  }
			  else
				{
				mPrintf("%c\n \n", hit);
				strcat(list[hit - 'A'], ".HLP");
				strcpy(nextfile, list[hit- 'A']);
				next= TRUE;
				errcount = 0;
				}
			  }
			  CurLine = 1;
		    }
	      fclose(file.fd);
	      }
    }
if (!(flags & HELP_LEAVE_PAGEABLE) && !(flags & HELP_NOT_PAGEABLE)) PagingOff();
return toReturn;
}

char getFileChar(HELP_FILE *file, int init)
{
char ret;

if (init)
   {
   file->aloop= -1;
   return file->aloop;
   }

/* still not re-entrant */
if (file->PB != 0) {
   ret = file->PB;
   file->PB = 0;
   return ret;
}

if (file->aloop == -1)
   {
   fgets(file->line, MAXWORD, file->fd);
   file->aloop= 0;
   }

input:
if ((ret= file->line[file->aloop++]) == 0)
	{
	if(!fgets(file->line, MAXWORD, file->fd))
			return 0;
	file->aloop= 0;
	goto input;
	}


if (outFlag == OUTSKIP) ret= 0;
return ret;
}

extern char more[];
#define INT_PTR		0
#define CHAR_PTR	1
#define FUNC_PTR	2
#define NO_ABORT	3
static struct {
	char *ourname;
	char type;
	union {
		UNS_16 *where;
		char   *addr;
		void   (*funcptr)(char *);
	} goo;
} VarNames[] = {
	{ "nodetitle", INT_PTR, &cfg.nodeTitle },
	{ "nodename", INT_PTR, &cfg.nodeName },
	{ "nodedomain", INT_PTR, &cfg.nodeDomain },
	{ "nodeid", INT_PTR, &cfg.nodeId },
	{ "baseroom", INT_PTR, &cfg.bRoom },
	{ "mainfloor", INT_PTR, &cfg.MainFloor },
	{ "variantname", CHAR_PTR, VARIANT_NAME },
	{ "more", CHAR_PTR, more },
	{ "sysopname", CHAR_PTR, cfg.SysopName },
	{ "ulprotocols", FUNC_PTR, UpProtsEnglish },
	{ "dlprotocols", FUNC_PTR, DownProtsEnglish },
	{ "doorlist", FUNC_PTR, DoorHelpListing },
	{ "noabort", NO_ABORT, NULL },
	{ "upoffline", FUNC_PTR, OfflineUp },
	{ "dloffline", FUNC_PTR, OfflineDown },
};

getHelpNames(HELP_FILE *file, char *name, char count, int flags)
{
char work[20], c, *t;
char buf[MAXWORD + 1];
int loop= 0, i;
extern MessageBuffer msgBuf;
SYS_FILE fn;

round:
while((buf[loop]= getFileChar(file, 0)) != '%' && buf[loop] && loop < MAXWORD-1) {
	if (buf[loop] == '^') {
		if (loop > MAXWORD - 50) {
			buf[loop] = 0;
			loop= 0;
			mPrintf("%s", buf);
		}
		i = 0;
		while(isalpha(work[i] = getFileChar(file, 0)) &&
							i < (sizeof work) - 1)
			i++;
		c = work[i];
		work[i] = 0;
/* printf("\nSearching on '%s'.\n", work); */
		for (i = 0; i < NumElems(VarNames); i++)
			if (strCmpU(VarNames[i].ourname, work) == SAMESTRING)
				break;
		if (i < NumElems(VarNames)) {
		    switch (VarNames[i].type) {
		    case NO_ABORT:
			outFlag = NO_CANCEL;
			t = "";
			break;
		    case INT_PTR:
			t = cfg.codeBuf + *VarNames[i].goo.where;
			break;
		    case CHAR_PTR:
			t = VarNames[i].goo.addr;
			break;
		    case FUNC_PTR:
			t = msgBuf.mbtext;
			(*VarNames[i].goo.funcptr)(msgBuf.mbtext);
			if (strlen(t) + loop > sizeof buf - 1) {
			    buf[loop] = 0;
		    	    mPrintf("%s", buf);
		    	    loop = 0;
			    if (strlen(msgBuf.mbtext) > sizeof buf) {
				mPrintf("%s", msgBuf.mbtext);
				msgBuf.mbtext[0] = 0;
			    }
			}
			break;
		    }
	    	buf[loop] = 0;
	    	strcat(buf, t);
	    	loop = strlen(buf);
		}
		else {
			for (loop++, i = 0; work[i] && loop < MAXWORD - 1;
								loop++, i++)
				buf[loop] = work[i];
		}
		buf[loop++] = c;
	}
	/*
	 * prevent words from being cut in half -- occasionally screws up
	 * formatting if we don't do this in long help files.
	 */
	else if (buf[loop] == ' ' && loop > MAXWORD - 12) {
	    break;
	}
	else
		loop++;
}
     if (!buf[loop])
	{
	mPrintf("%s", buf);
	return FALSE;
	}
/*     if(loop == MAXWORD-1) */
     if (buf[loop] != '%')
	{
	     buf[loop+1]= 0;
	     loop= 0;
	     mPrintf("%s", buf);
	     goto round;
	     }

     buf[loop]= 0;
     if (outFlag != NO_CANCEL) outFlag = OUTOK;
     mPrintf("%s", buf);
     loop= 1;		/* crafty initialization */
     if ((name[0]= getFileChar(file, 0)) == '%') {
	buf[0] = '%';
	goto round;	/* yecccch -- sometime Paul's code should be
				rewritten */
     }
     while((name[loop]= getFileChar(file, 0)) != ' ' && name[loop] && loop <= 7)
	   loop++;
    name[loop]= 0;
    sprintf(work, "%s.hlp", name);
    makeSysName(fn, work, &cfg.homeArea);
    if ((HELP_NO_LINKAGE & flags) || access(fn, 0) != 0) {
	/*
	 * don't show the file name or its comment -- so the unwary won't
	 * stumble into nothingness.
	 */
	while ((c = getFileChar(file, 0)) != 0 && c != '\n')
		;
	loop= 0;
	buf[0]= 0;
	/*
	 * This bizarre little if with a pushback is the result of noticing
	 * when help files are missing, the help file trying to find them
	 * will pile up the leading spaces which characterize most help
         * files in the " %MYHELP" stuff.  This makes the listing of help
	 * files to jump to "lumpy."  The if right here takes care of that
	 * by peeking ahead and killing a leading space if it's there.  This
	 * is obviously not a perfect solution, but hopefully the
	 * imperfections will rarely show up.
	 */
	if ((c = getFileChar(file, 0)) != ' ')
		file->PB = c;
	goto round;	/* eeeeeeeyyyyyyyuuuuuuuuuucccccccckkkk! */
    }
    mPrintf("[%c] %-8s  ", 'a' + count, name);
    return TRUE;
}

/**************************** EOS (end of source) ****************************/
