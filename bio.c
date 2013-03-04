/*
 *				bio.c
 *
 * User Biography Functions.
 */

/*
 *				history
 *
 * 91Jun14 HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	BioDirectory()		Gets a list of user biographies.
 *	EditBio()		Edits a biography by a user.
 *	GetBioInfo()		Gets a bio from disk.
 *	SaveBioInfo()		Save a bio to disk.
 */
extern CONFIG      cfg;		/* Lots an lots of variables    */
extern logBuffer   logBuf;		/* Person buffer		*/
extern logBuffer   logTmp;		/* Person buffer		*/
extern MessageBuffer     msgBuf;
extern char	   haveCarrier;    /* Do we still got carrier?     */
extern int	   thisLog;
extern char	   *LCHeld, *WRITE_ANY, *READ_ANY;
extern char	   outFlag;	   /* Output flag			*/
extern char	   onConsole;

/*
 * EditBio()
 *
 * This function allows the editing of a biography.  If a biography already
 * exists then an option is offered to edit that rather than create a new
 * biography.  NB: "null" biographies are not saved but are rather completely
 * deleted so they don't show up on .M?
 */
void EditBio()
{
    char name[15];
    SYS_FILE bio;

    doCR();
    sprintf(name, "%d.bio", thisLog);
    makeSysName(bio, name, &cfg.bioArea);
    if (access(bio, 0) == 0) {
	if (getYesNo("Edit current biography")) {
	    GetBioInfo(thisLog);
	}
	else msgBuf.mbtext[0] = 0;
    }
    else msgBuf.mbtext[0] = 0;

    if (!expert)
	printHelp("bionov.blb", HELP_SHORT);

    printHelp("biossys.blb", 0);
    mPrintf("\n    Biography");
    doCR();
    CleanEnd(msgBuf.mbtext);
    mPrintf("%s", msgBuf.mbtext);
    outFlag = OUTOK;
    if (GetBalance(ASCII,msgBuf.mbtext,MAXTEXT-50,BIO_ENTRY, "") && onLine()) {
	CleanEnd(msgBuf.mbtext);
	if (strLen(msgBuf.mbtext) < 3)
	    ClearBio(thisLog);
	else SaveBioInfo(thisLog);
    }
}

/*
 * GetBioInfo()
 *
 * This function handles the mechanics of getting biographical info.  It
 * handles the encryption.
 */
char GetBioInfo(int which)
{
    char name[15];
    SYS_FILE bio;
    FILE *fd;
    long size;

    sprintf(name, "%d.bio", which);
    makeSysName(bio, name, &cfg.bioArea);
    if ((fd = fopen(bio, READ_ANY)) != NULL) {
	totalBytes(&size, fd);
	fread(msgBuf.mbtext, (int) size, 1, fd);
	crypte(msgBuf.mbtext, (int) size, which);
	fclose(fd);
	return TRUE;
    }
    msgBuf.mbtext[0] = 0;
    return FALSE;
}

/*
 * SaveBioInfo()
 *
 * This function handles the mechanics of saving biographical info.  This
 * includes encrypting the bio before saving it.
 */
void SaveBioInfo(int which)
{
    char name[15];
    SYS_FILE bio;
    FILE *fd;
    int size;

    sprintf(name, "%d.bio", which);
    makeSysName(bio, name, &cfg.bioArea);
    if ((fd = fopen(bio, WRITE_ANY)) != NULL) {
	size = strLen(msgBuf.mbtext) + 1;	/* include NULL byte */
	crypte(msgBuf.mbtext, (int) size, which);
	fwrite(msgBuf.mbtext, (int) size, 1, fd);
	crypte(msgBuf.mbtext, (int) size, which);
	fclose(fd);
    }
    else msgBuf.mbtext[0] = 0;
}

/*
 * BioDirectory()
 *
 * This shows who's written biographies.  Rather ugly since we're between
 * major releases.  If we do another major release we should clean out
 * that call to MoveToBioDirectory().
 */
void BioDirectory()
{
    void ShowBioName(DirEntry *str);

    MoveToBioDirectory();
    if (wildCard(ShowBioName, "*.bio", "", WC_NO_COMMENTS) == 0) {
	mPrintf("There are currently no biographies listed.\n ");
    }
    else mPrintf("\b\b. \n ");
    homeSpace();
}

/*
 * ShowBioName()
 *
 * This internal function shows the name of a person with a bio.  It is called
 * in connection with RunList() (see above).
 */
static void ShowBioName(DirEntry *entry)
{
    char *dot;

    if ((dot = strchr(entry->unambig, '.')) == NULL) return;
    *dot = 0;
    getLog(&logTmp, atoi(entry->unambig));
    if (logTmp.lbflags.L_INUSE)
	mPrintf("%s, ", logTmp.lbname);
}

