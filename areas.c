/*
 *				areas.c
 *
 * area code for Citadel bulletin board system.
 */

/*
 *				history
 *
 *	SEE THE INCREM.* FILES FOR FURTHER HISTORICAL NOTES
 *
 * 89Oct12 HAW  Created from rooma.c.
 */

#include "ctdl.h"
#include "compress.h"
#include <alloc.h>

/*
 *				Contents
 *
 *	DirCheck()		Does this entry already exist in list?
 *	DirCmp()		Used to sort directory entries.
 *	DirFree()		frees a directory entry
 *	fDir()                  prints out a filename for a dir listing
 *	ShowVerbose()		display of a file for .Read Extended
 *	wildCard()              expands ambiguous filenames
 */

long          FDSize;              /* For accumulating dir. size   */
char *DirFileName = "ctdldir.sys";
SListBase DirBase  = { NULL, ChkNtoStr, NULL, FreeNtoStr, EatNMapStr };

extern CONFIG    cfg;            /* A buncha variables           */
extern MessageBuffer   msgBuf;
extern char      outFlag;
extern aRoom     roomBuf;        /* room buffer                  */
extern AN_UNSIGNED crtColumn; /* current position on screen           */
extern logBuffer logBuf;         /* Buffer for the pippuls       */
extern int       thisRoom;


/*
 * fDir()
 *
 * This function prints out one filename and size for a dir listing.  This
 * is the .RD function.
 */
void fDir(DirEntry *file)
{
    long Sectors;

    if (outFlag != OUTOK) return;
    Sectors   = ((file->FileSize + 127) / SECTSIZE);
    FDSize   += Sectors;

    mPrintf("%-15s%5ld%2c", file->unambig, Sectors, ' ');
    mAbort();       /* chance to next(!)/pause/skip */
}

/*
 * ShowVerbose()
 *
 * This function does a display of a file for .Read Extended.
 */
void ShowVerbose(DirEntry *file)
{
    extern int DirAlign;
    extern char AlignChar;
    char *strchr(), *c, found, funnyflag = FALSE, sbuf[100];
    char work[30], *author;
    int  format;
    FILE *fd;
    extern char *READ_ANY;
    extern FunnyInfo Formats[];

    if (outFlag != OUTOK) return;

    FDSize += file->FileSize;

    found = FindFileComment(file->unambig, TRUE);

#ifndef TURBO_C_VSPRINTF_BUG
    mPrintf("%-*s%7s ", MAX_FILENAME + 2, file->unambig,
        PrintPretty(file->FileSize, work));
#else
    mPrintf("%s", file->unambig);
    SpaceBug(MAX_FILENAME + 2 - strLen(file->unambig));
    mPrintf("%7s ", PrintPretty(file->FileSize, work));
#endif

    format = CompressType(file->unambig);
    if (logBuf.lbflags.ALT_RE) {
	mPrintf("|");
	if (found) {
	    if ((c = strchr(msgBuf.mbtext, '\n')) != NULL) *c = 0;
	    DirAlign = 25;
	    AlignChar = '|';
	    mFormat(strchr(msgBuf.mbtext, ' '), oChar, doCR);
	}

	if (format != ERROR && !Formats[format].Many &&
					Formats[format].Func != NULL) {
	    fd = fopen(file->unambig, READ_ANY);
	    mPrintf(" [");
	    (*Formats[format].Func)(fd, FALSE, sbuf);
	    mPrintf("%s]", sbuf);
	    fclose(fd);
	}

	mPrintf(" (%s)", file->FileDate);
    }
    else {

	mPrintf(" %s", file->FileDate);
	if (format != ERROR && !Formats[format].Many &&
					Formats[format].Func != NULL) {
	    fd = fopen(file->unambig, READ_ANY);
	    mPrintf(" [");
	    (*Formats[format].Func)(fd, FALSE, sbuf);
	    mPrintf("%s]", sbuf);
	    funnyflag = TRUE;
	    fclose(fd);
	}

    /* the strlen check lets us handle "empty" entries -- found, but */
    /* really nothing there. */
	if ((c = strchr(msgBuf.mbtext, '\n')) != NULL) *c = 0;
	if (found && strlen(strchr(msgBuf.mbtext, ' ')) + crtColumn < logBuf.lbwidth)
	    mFormat(strchr(msgBuf.mbtext, ' '), oChar, doCR);
	else if (found && strlen(strchr(msgBuf.mbtext, ' ')) > 2) {
	    /* first, we try to parse out author. */
	    c = lbyte(msgBuf.mbtext) - 2;
	    if (strCmp(c, "].") == SAMESTRING &&
		    (author = strrchr(msgBuf.mbtext, '[')) != NULL) { /* yes! */
		*author++ = 0;	/* null out comment's upload author */
		*c = 0;		/* kill "]." */
		mPrintf(" %sby %s.", (funnyflag) ? "" : "Uploaded ", author);
	    }
	    mPrintf("\n   |");
	    DirAlign = 3;
	    AlignChar = '|';
	    mFormat(strchr(msgBuf.mbtext, ' '), oChar, doCR);
	}
    }

    DirAlign = 0;
    doCR();
    mAbort();       /* chance to next(!)/pause/skip */
}

/*
 * wildCard()
 *
 * This function allows generic access to a directory room. The actual actions
 * take place via the fn function pointer.  If we're already in the required
 * directory area then needToMove should be FALSE.  AssumeDefault is used
 * if filename is 0 length -- the *.* or * or whatever is appropriate for
 * the system is used to find all the files in the directory.
 */
int wildCard(void (*fn)(DirEntry *str), char *filename, char *phrase, int flags)
{
    int   realCount;
    SListBase LocFiles = { NULL, DirCheck, DirCmp, DirFree, NULL };
    char  *DatePtr, *work;
    long  before = -1l, after = -1l;

/* printf("coreleft at 1 is %ld\n", coreleft()); */
    if (flags & WC_MOVE)
        if (!SetSpace(FindDirName(thisRoom))) {
            return 0;
        }

/* printf("coreleft at 2 is %ld\n", coreleft()); */
    work = strdup(filename);

/* printf("coreleft at 3 is %ld\n", coreleft()); */
    if ((DatePtr = DateSearch(work, &before, &after)) != NULL)
        DateSearch(DatePtr, &before, &after);
/* printf("coreleft at 4 is %ld\n", coreleft()); */

    if ((flags & WC_DEFAULT) && strLen(work) == 0) {
        free(work);
	work = strdup(ALL_FILES);
    }

/* printf("coreleft at 5 is %ld\n", coreleft()); */
    CitGetFileList(work, &LocFiles, before, after, phrase);
/* printf("coreleft at 6 is %ld\n", coreleft()); */
    realCount = wild2Card(&LocFiles, fn, flags);
/* printf("coreleft at 6.1 is %ld\n", coreleft()); */
    KillList(&LocFiles);
/* printf("coreleft at 7 is %ld\n", coreleft()); */
    if (flags & WC_MOVE) homeSpace();
    free(work);
/* printf("coreleft at 8 is %ld\n", coreleft()); */
    return realCount;
}

/*
 * wild2Card()
 *
 * integrate this back into wildCard() someday.
 */
static int wild2Card(SListBase *Files, void (*fn)(), int flags)
{
	int Count;
	DirEntry entry;

	outFlag     = OUTOK;
	if (!(flags & WC_NO_COMMENTS)) {
#ifdef COMMENT_HEADER
		if (access(COMMENT_HEADER, 0) == 0) {
			entry.unambig = COMMENT_HEADER;
			doFormatted(&entry);
			doCR();
		}
#endif
		StFileComSearch();
	}
	Count = RunList(Files, fn);
	if (!(flags & WC_NO_COMMENTS)) EndFileComment();
	return Count;
}

/*
 * FindDirName()
 *
 * This finds the directory associated with some room.
 */
char *FindDirName(int roomNo)
{
    return (char *)
		SearchList(&DirBase, NtoStrInit(roomNo, "", 0, TRUE));
}

/*
 * getSize()
 *
 * This function gets the size of the file from the actual directory entry.
 */
void getSize(DirEntry *file)
{
    extern long netBytes;

    netBytes += file->FileSize;
}

/*
 * DirCheck()
 *
 * Does this entry already exist in the list?  If so, return a pointer to
 * the data that matched the data in the list, otherwise NULL.  This is used
 * in the list built by wildCard() to cull out duplicate entries, so we
 * return s2 rather than s1.
 */
void *DirCheck(DirEntry *s1, DirEntry *s2)
{
    return (strCmpU(s1->unambig, s2->unambig) == 0) ? s2 : NULL;
}

/*
 * DirCmp()
 *
 * This is used by Slist to sort directory entries.
 */
static int DirCmp(DirEntry *s1, DirEntry *s2)
{
    return strCmpU(s1->unambig, s2->unambig);
}

/*
 * DirFree()
 *
 * This function frees a directory entry.
 */
static void DirFree(DirEntry *d)
{
    free(d->unambig);
    free(d);
}

