/*
 *				sysarc.c
 *
 * This file contains the system dependent code for decompressing files for
 * download.
 */

/*
 *				history
 *
 * 91Oct07 HAW	Netcache stuff.
 * 88Jun08 HAW	Created.
 */

#define SYSTEM_DEPENDENT

#include "ctdl.h"
#include "sys\stat.h"
#include "process.h"

/*
 *				Contents
 *
 *	ArcInit()		initialize for decompressing
 *	CompAvailable()		is compression available for this type?
 *	SendArcFiles()		get arc name, send contents specified
 * #	MakeTempDir()		Create and move into a temporary dir
 *
 *		# == local for this implementation only
 */

extern aRoom roomBuf;
extern MessageBuffer msgBuf;
extern CONFIG  cfg;
extern char	whichIO;
extern char	   onConsole, haveCarrier;

char TDirBuffer[120];

/*
 * This table must be kept in sync with Formats[] in Misc.c
 */
DeCompElement DeComp[] = {
	{ /* "compress.lzh", */ "Lzh",   NULL, NULL, NULL, NULL },
	{ /* "compress.zip", */ "PKZip", NULL, NULL, NULL, NULL },
	{ /* "compress.zoo", */ "Zoo",   NULL, NULL, NULL, NULL },
	{ /* "compress.arc", */ "Arc",   NULL, NULL, NULL, NULL },
};

void CheckCompressedData(DirEntry *name);
void MakeTempName(void);

void *FindCompFormat();
static SListBase CF = { NULL, FindCompFormat, NULL, NULL, NULL };

/*
 * ArcInit()
 *
 * This initializes this module for handling the various variants of
 * compression/decompression.
 */
void ArcInit()
{
    SYS_FILE name;

    sprintf(name, "%c:%s", cfg.roomArea.saDisk + 'a',
				cfg.codeBuf + cfg.roomArea.saDirname);

    if (name[strlen(name) - 1] == '\\')	/* yech */
	name[strlen(name) - 1] = 0;

    SetSpace(name);
    wildCard(CheckCompressedData, "compress.*", "", WC_NO_COMMENTS);
    homeSpace();
}

/*
 * CheckCompressedData()
 *
 * This function does an initial check for existence of a configuration file.
 */
static void CheckCompressedData(DirEntry *name)
{
    FILE *fd;
    char work[50], *c;
    DeCompElement *CompressRecord;
    int index;

    if ((fd = fopen(name->unambig, "r")) == NULL) {
	return;
    }

    if ((index = CompressType(name->unambig)) == -1) {
	CompressRecord = GetDynamic(sizeof *CompressRecord);
    }
    else CompressRecord = DeComp + index;
    if ((c = strrchr(name->unambig, '.')) != NULL)
	CompressRecord->Suffix = strdup(c+1);

    if (GetAString(work, sizeof work, fd) != NULL) {
	if (strlen(work) != 0) CompressRecord->DeWork = strdup(work);
	if (GetAString(work, sizeof work, fd) != NULL) {
	    if (strlen(work) != 0) CompressRecord->IntWork = strdup(work);
	    if (GetAString(work, sizeof work, fd) != NULL)
		if (strlen(work) != 0) CompressRecord->CompWork = strdup(work);
		if (GetAString(work, sizeof work, fd) != NULL)
		    if (strlen(work) != 0) CompressRecord->TOC = strdup(work);
	}
    }

    AddData(&CF, CompressRecord, NULL, FALSE);

    fclose(fd);
}

/*
 * SendArcFiles()
 *
 * This function gets arc filename, send specified contents.
 */
void SendArcFiles(int protocol)
{
    SYS_FILE name;
    char     *temp, IsRoot;
    struct stat statbuff;
    int  NumFiles;
    extern int thisRoom;
    void *CheckSuff();
    extern long netBytes;
    DeCompElement *CR;
    extern char *UploadLog;

    getNormStr("file to decompress from", name, SIZE_SYS_FILE, 0);

    if (strLen(name) == 0) return;

    if (!SetSpace(FindDirName(thisRoom))) return;

    temp = getcwd(NULL, 100);
    IsRoot = (temp[strLen(temp) - 1] == '\\');
    sprintf(msgBuf.mbtext, (IsRoot) ? "%s%s" : "%s\\%s", temp, name);
    homeSpace();

    if (access(msgBuf.mbtext, 4) == -1) {
	if ((CR = AltSearchList(&CF, CheckSuff, msgBuf.mbtext)) == NULL)
	    CR = DeComp;	/* i.e., element 0 of DeComp */
    }
    else if ((CR = SearchList(&CF, strrchr(msgBuf.mbtext, '.') + 1)) == NULL) {
	CR = DeComp;	/* i.e., element 0 of DeComp */
    }

    if (stat(msgBuf.mbtext, &statbuff) != 0 ||
	statbuff.st_mode & S_IFCHR ||
	statbuff.st_mode & S_IFDIR ||
	!(statbuff.st_mode & S_IREAD)) {
	mPrintf("%s does not exist.\n ", name);
	free(temp);
	return;
    }

    if (CR->DeWork == NULL) {
	mPrintf("Sorry, de%sing not supported here.\n ",
						CR->MenuEntry);
	return;
    }

    getNormStr("decompress mask", msgBuf.mbtext, 100, 0);

    if (strLen(msgBuf.mbtext) == 0) strCpy(msgBuf.mbtext, ALL_FILES);

    MakeTempDir();      /* Create and drop into */

    mPrintf("One moment, please ...\n ");

    CitSystem(!cfg.BoolFlags.debug, (IsRoot) ? "%s %s%s %s" : "%s %s\\%s %s",
			CR->DeWork, temp, name, msgBuf.mbtext);

    free(temp);         /* Don't need this any longer, so kill it now */

    netBytes = 0l;

    NumFiles = wildCard(getSize, ALL_FILES, "", WC_NO_COMMENTS);

    if (NumFiles <= 0) {
	mPrintf("Sorry, no files matched that decompress command.\n ");
    }
    else {
	if (netBytes <= statbuff.st_size || getYesNo(
"It would be more efficient to download the compressed file, continue anyways"))
	    if (onLine() && TranAdmin(protocol, NumFiles)) {
		TranSend(protocol, transmitFile, ALL_FILES, "", FALSE);
		unlink(UploadLog);
	    }
	/*
	 * This call is due to a regretable side-effect of losing
	 * carrier while reading compress-derived files.
	 */
	chdir(TDirBuffer);
	wildCard(DelFile, ALL_FILES, "", WC_NO_COMMENTS);
    }
    homeSpace();
    rmdir(TDirBuffer);
}

/*
 * CheckSuff()
 *
 * This odd little function searches the list of decompression types that
 * this installation supports to see if the given file matches any of the
 * suffixes.  It so, it sets the filename in base to the complete filename
 * and returns this element.
 */
static void *CheckSuff(DeCompElement *element, char *base)
{
    char *dup, toReturn;

    dup = strdup(base);
    strcat(base, ".");
    strcat(base, element->Suffix);
    toReturn = (access(base, 4) == 0);
    if (!toReturn) strcpy(base, dup);
    free(dup);
    if (toReturn) return element;
    return NULL;
}

/*
 * MakeTempDir()
 *
 * This function will create and move into a temporary directory.
 */
void MakeTempDir()
{
    MakeTempName();

    if (mkdir(TDirBuffer) != 0)
	mPrintf("System error with mkdir!\n ");
    chdir(TDirBuffer);
    getcwd(TDirBuffer, 120);
}

/*
 * MakeTempName()
 *
 * This function gets a temporary name.
 */
static void MakeTempName()
{
	static char *seed = "8ys6%d";
	int i = 0;

	sprintf(TDirBuffer, seed, i++);
	while (access(TDirBuffer, 0) != -1)
		sprintf(TDirBuffer, seed, i++);
}

#define BAT_FILE	\
"%s\nif errorlevel 1 goto bad\nif errorlevel 0 goto good\n\
:bad\n\
echo 1 >%s\n\
exit\n\
:good\n\
echo 0 >%s\n\
exit\n"

/*
 * FileIntegrity()
 *
 * This function does a file integrity check.
 * Note: we don't use spawn() since it adds 3K to the .EXE.
 */
char FileIntegrity(char *filename)
{
    char *c, *d, check[90], bad = TRUE, NameUsed = FALSE;
    FILE *fd;
    DeCompElement *CR;

    if ((c = strrchr(filename, '.')) != NULL) {
	if ((CR = SearchList(&CF, c+1)) == NULL) return TRUE;

	if (CR->IntWork == NULL) return TRUE;

	if (!getYesNo("Do you want to check the integrity of your upload"))
		return TRUE;

	mPrintf("Please be patient ...");

	MakeTempName();
	strCat(TDirBuffer, ".bat");
	fd = fopen(TDirBuffer, "w");
	strCpy(check, TDirBuffer);
	MakeTempName();

	/*
	 * Manually copy.  If we encounter "%g" then replace it with the
	 * name of the file to test.  If we never encounter it, then
	 * remember to append it to the end of the string after the copy
	 * has finished (NameUsed).
	 */
	for (c = CR->IntWork, d = msgBuf.mbtext; *c; c++) {
	    if (*c == '%' && *(c + 1) == 'g') {
		strCpy(d, filename);
		NameUsed = TRUE;
		while (*d) d++;
		c++;			/* this will get us over the %g. */
	    }
	    else *d++ = *c;
	}
	*d++ = ' ';		/* harmless space padding. */
	*d = 0;

	if (!NameUsed) strCat(msgBuf.mbtext, filename);

	fprintf(fd, BAT_FILE, msgBuf.mbtext, TDirBuffer, TDirBuffer);
	fclose(fd);
	CitSystem(TRUE, "%s", check);
	unlink(check);

	if ((fd = fopen(TDirBuffer, "r")) != NULL) {
	    if (fgets(check, sizeof check, fd) != NULL) {
		bad = (strncmp(check, "1", 1) == SAMESTRING);
	    }
	    fclose(fd);
	    unlink(TDirBuffer);
	    if (bad)
		return !getYesNo("Failed integrity check; abort upload");
	    else
		mPrintf("\n Passed integrity check.\n ");
	}
    }
    return TRUE;
}

/*
 * CompAvailable()
 *
 * This function determines if the given compression type has a compression
 * method defined.
 */
char CompAvailable(char CompType)
{
    return (DeComp[CompType - 1].CompWork != NULL);
}

/*
 * NetDeCompress()
 *
 * This decompresses files for network dissection.
 */
void NetDeCompress(char CompType, SYS_FILE fn)
{
	CitSystem(TRUE, "%s %s", DeComp[CompType - 1].DeWork, fn);
}

/*
 * KillNetDeCompress()
 *
 * This function clears out the files from the decompression.
 * We assume we're IN the dir to be cleared.
 */
void KillNetDeCompress(char *dir)
{
	wildCard(DelFile, ALL_FILES, "", WC_NO_COMMENTS);
	homeSpace();
	rmdir(dir);
}

/*
 * Compress()
 *
 * This compresses the given files into the given archive file using the
 * given file.
 */
void Compress(char CompType, char *Files, char *CompName)
{
    if (!CompAvailable(CompType)) return;

    CitSystem(TRUE, "%s %s %s", DeComp[CompType - 1].CompWork, CompName, Files);
}

/*
 * GetUserCompression()
 *
 * This function handles a request for a compression protocol.  This can be
 * called either for a sysop (when selecting for Mass Transfers) or by a user
 * (eventually) for .RC.
 *
 * This returns the protocol selected (LHA_COMP, etc) or NO_COMP if none
 * was selected or if none are available.
 */
int GetUserCompression()
{
	int  rover, count = 0, x;
	MenuId   id;
	char *CompOpts[] = {
		" ", " ", " ", " ", ""
	};

	if (!onConsole || cfg.DepData.OldVideo)
		mPrintf("\n ");

	for (rover = 0; rover < NumElems(DeComp); rover++) {
		if (DeComp[rover].CompWork != NULL) {
			count++;
			ExtraOption(CompOpts, DeComp[rover].MenuEntry);
			if (whichIO == MODEM || cfg.DepData.OldVideo)
				mPrintf("%s\n ", DeComp[rover].MenuEntry);
		}
	}

	if (count == 0) {
		SysopError(NO_MENU, "No Compression methods");
		return NO_COMP;
	}

	id = RegisterSysopMenu("", CompOpts, " Compression ", 0);
	SysopMenuPrompt(id, "\n Compression method: ");
	x = GetSysopMenuChar(id);
	CloseSysopMenu(id);

	switch (x) {
		case 'P': return ZIP_COMP;
		case 'Z': return ZOO_COMP;
		case 'L': return LHA_COMP;
		case 'A': return ARC_COMP;
		default:  return NO_COMP;
	}
}

/*
 * FindCompFormat()
 *
 * This function helps find a decompression record based on the given suffix.
 */
static void *FindCompFormat(DeCompElement *cr, char *suffix)
{
    if (strCmpU(cr->Suffix, suffix) == 0) return cr;
    return NULL;
}

void ReadExternalDir(char *name)
{
    extern PROTO_TABLE Table[];
    extern int	DirAlign;
    extern char	AlignChar;
    FILE *fd;
    char *c;
    DeCompElement *CR;

    if ((c = strrchr(name, '.')) == NULL) return;

    if ((CR = SearchList(&CF, c + 1)) == NULL) return;

    if (CR->TOC == NULL) return;

    mPrintf("\n %s", name);
    if (FindFileComment(name, FALSE)) {
	DirAlign = strLen(name) + 3;
	AlignChar = 0;
	mPrintf(":%s", strchr(msgBuf.mbtext, ' '));
	DirAlign = 0;
    }

    mPrintf("\n ");

    MakeTempName();

    mPrintf("One moment ...");
    CitSystem(TRUE, "%s %s > %s", CR->TOC, name, TDirBuffer);

    if ((fd = fopen(TDirBuffer, READ_TEXT)) != NULL)
	SendThatDamnFile(fd, Table[0].method);

    unlink(TDirBuffer);
}
