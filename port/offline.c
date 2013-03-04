/*
 *				offline.c
 *
 * Offline Reader support.
 */

/*
 *				history
 *
 * 94Jan18 HAW	Created.
 */

#define NET_INTERFACE
#define NET_INTERNALS
#define USER_INTERFACE

#include "ctdl.h"

/*
 *				contents
 *
 */

#define DROP		"DROP.SYS"

extern CONFIG	   cfg;		/* Lots an lots of variables    */
extern logBuffer   logBuf;		/* Person buffer		*/
extern logBuffer   logTmp;		/* Person buffer		*/
extern aRoom	   roomBuf;		/* Room buffer		*/
extern rTable	   *roomTab;
extern MessageBuffer     msgBuf;
extern MessageBuffer     tempMess;
extern NetBuffer   netBuf;
extern int	   outPut;
extern char	   onConsole;
extern AN_UNSIGNED crtColumn;	/* where are we on screen now?	*/
extern char	   loggedIn;	/* Is we logged in?			*/
extern char	   outFlag;	   /* Output flag			*/
extern char	   haveCarrier;    /* Do we still got carrier?     */
extern char	   heldMess;
extern int	   TransProtocol;  /* transfer protocol in use     */
extern char	   prevChar;	/* previous char output		*/
extern char	   *READ_ANY, *READ_TEXT;
extern char	   textDownload;   /* flag	   */
extern int	   thisRoom;
extern int	   thisLog;
extern char	   whichIO;	   /* Where I/O is	   */
extern char	   echo;	   /* Should we echo? echo? echo?  */
extern FILE	   *msgfl;
extern FILE	   *roomfl;
extern FILE	   *logfl;
extern int	   exitValue;
extern char	   *LCHeld, *WRITE_ANY, *WRITE_TEXT;
extern char	   PrintBanner;
extern char	   *R_SH_MARK, *LOC_NET, *NON_LOC_NET;

void *FindOfflineReader(), *EatOfflineReader(char *line);
SListBase OfflineReaderDown = { NULL, FindOfflineReader, NULL, NULL, EatOfflineReader };
SListBase OfflineReaderUp = { NULL, FindOfflineReader, NULL, NULL, NULL };

char *Site;
char *Basename;
/*
 * OffLineInit()
 *
 * This function initializes the data structure that knows about the offline
 * reading support for this installation.
 */
void OffLineInit()
{
	SYS_FILE fn;
	FILE *fd;

	makeSysName(fn, OFF_DEFS, &cfg.roomArea);
	if ((fd = fopen(fn, READ_TEXT)) != NULL) {
		GetAString(msgBuf.mbtext, 500, fd);
		Site = strdup(msgBuf.mbtext);
		GetAString(msgBuf.mbtext, 500, fd);
		Basename = strdup(msgBuf.mbtext);
		MakeList(&OfflineReaderDown, "", fd);
		fclose(fd);
	}
}

/*
 * FindOfflineReader()
 *
 * This function helps find an offline reader in a list.
 */
static void *FindOfflineReader(OfflineReader *data, char *sel)
{
	if ((*sel) == data->Selector) return data;
	return NULL;
}

/*
 * EatOfflineReader()
 *
 * This function eats a line from ctdloff.sys.
 *
 * NB: this function serves both the Up and Down lists, but is only called for
 * one list.  This function makes its decision based on the t | r which is the
 * first token of the offline reader configuration.
 */
static void *EatOfflineReader(char *line)
{
	char *tok, Down, work[50];
	OfflineReader *d;

	/* gets the t | r token, used to determine which list to put this in. */
	tok = strtok(line, " \t\n");
	if (tok == NULL) return NULL;
	if (toUpper((*tok)) == 'T')
		Down = TRUE;
	else if (toUpper((*tok)) == 'R')
		Down = FALSE;
	else return NULL;

	d = GetDynamic(sizeof *d);

	/* this should be the selector */
	tok = strtok(NULL, " \t\n");
	if (tok != NULL) {
		d->Selector = *tok;

		/* this should be the name of the OfflineReader */
		tok = strtok(NULL, " \t\n");
		if (tok != NULL) {
			d->Name = strdup(tok);
			/* this should be the command line of the OfflineReader */
			tok = strtok(NULL, "\n");
			if (tok != NULL) {
				d->Cmd = strdup(tok);
				sprintf(work,
					"%s%c\b%s", (Down) ? NTERM : TERM,
							d->Selector,d->Name);
				d->Display = strdup(work);
				if (Down) {
					return d;
				}
				else {
					AddData(&OfflineReaderUp, d, NULL, FALSE);
					return NULL;
				}
			}
			else free(d->Name);
		}
	}
	free (d);
	return NULL;
}

/*
 * AddOfflineReaderOptions()
 *
 * This function adds external protocol options to system menus.
 */
void AddOfflineReaderOptions(char **Opts, char upload)
{
	void AddOfflineReaderOpts();

	RunListA(upload ? &OfflineReaderUp : &OfflineReaderDown,
			AddOfflineReaderOpts, (void *) Opts);
}

/*
 * AddOfflineReaderOpts()
 *
 * This function does the actual work of adding an option to a menu list.
 */
static void AddOfflineReaderOpts(OfflineReader *d, char **TheOpts)
{
	ExtraOption(TheOpts, d->Display);
}

/*
 * DropFile()
 *
 * This function creates the DROP file.  It returns TRUE on success, FALSE
 * on failure.
 */
int DropFile(int dir, char *tempdir)
{
	FILE *fd;
	extern char *VERSION;
	int rover, count;
	extern char NotForgotten;
	char audit[200];

	if ((fd = fopen(DROP, WRITE_TEXT)) != NULL) {
		fprintf(fd, "#DIRECTION %d\n#BBSFILENAME %s\n", dir, Basename);
		fprintf(fd, "#WORKDIR %s\n#BBSNAME %s\n", tempdir,
						cfg.codeBuf + cfg.nodeName);
		fprintf(fd, "#SYSOP %s\n#CITYNAME %s\n", cfg.SysopName, Site);
		if (cfg.Audit != 0) {
			SysArea(audit, &cfg.auditArea);
			fprintf(fd, "#AUDIT %s\n", audit);
		}
		fprintf(fd, "#CITID %s v%s\n#NETID %s\n", VARIANT_NAME, VERSION,
						cfg.codeBuf + cfg.nodeId);
		fprintf(fd, "#USER %s\n", logBuf.lbname);

		NotForgotten = TRUE;
		for (rover = count = 0; rover < MAXROOMS; rover++) {
			if (KnownRoom(rover)) count++;
		}
		fprintf(fd, "#ROOMS %d\n", count);
		for (rover = count = 0; rover < MAXROOMS; rover++) {
			if (KnownRoom(rover)) {
				fprintf(fd, "#ROOMNUM%d %d\n", count, rover);
				fprintf(fd, "#ROOMNAME%d %s\n", count,
							roomTab[rover].rtname);
				count++;
			}
		}
		fclose(fd);
		return TRUE;
	}
	return FALSE;
}

/*
 * ORWriteMsg()
 *
 * This function writes a message to disk for offline reader processing.
 * It is used with showMessage.
 */
char ORWriteMsg(int mode, int slot)
{
	if (OptionCheck(mode, slot)) {
		prNetStyle(FALSE, getMsgChar, putFLChar, TRUE, "");
		return TRUE;
	}
	return FALSE;
}

#define OFFRES		"offres.sys"
/*
 * OR_Result()
 *
 * This function examines the results of a offline reader call.  It returns
 * TRUE on success, FALSE on failure, and takes care of echoing results to the
 * user.
 */
int OR_Result(char *filename)
{
	FILE *fd;
	char *err = "Offline reader translator failed, operation aborted.\n ";
	char line[50];
	int res;

	if ((fd = fopen(OFFRES, READ_TEXT)) != NULL) {
		if (GetAString(line, sizeof line, fd) == NULL) {
			mPrintf(err);
		}
		if ((res = atoi(line)) == 0) {
			mPrintf(err);
		}
		/* filename to compress to */
		GetAString(filename, 20, fd);
		while (fgets(line, sizeof line, fd) != NULL) {
			mPrintf("%s", line);
		}
		fclose(fd);
		unlink(OFFRES);
		unlink(DROP);
		return res;
	}
	else {
		mPrintf(err);
		return FALSE;
	}
}

int offline_mode;
/*
 * OR_Upload()
 *
 * This function deals with Offline Reader uploads.
 */
void OR_Upload(OfflineReader *Reader, char Protocol)
{
	extern FILE *upfd;
	extern char *UploadLog;
	char successful, useless[20];
	char Compression;
	extern SListBase FileList;
	char name[30];
	SYS_FILE msgs;
	UploadFile *entry;
	void AddOffLineMsg(void);

	if ((Compression = GetUserCompression()) == NO_COMP) {
		return;
	}

	if (Protocol == ASCII) {
		mPrintf("Xmodem assumed.\n ");
	}

	if (!getYesNo("Ready")) {
		return;
	}

	ToTempArea();
	sprintf(name, "%s.", Basename);

	if (!InternalProtocol(Protocol)) {
		successful = (ExternalProtocol(Protocol, TRUE, name, NULL, FALSE)
						== TRAN_SUCCESS);
		if (DoesNumerous(Protocol)) {
			offline_mode = TRUE;
			MakeList(&FileList, UploadLog, NULL);
			offline_mode = FALSE;
			unlink(UploadLog);
			if (RunList(&FileList, NoFree) != 1) {
				mPrintf("Too many files uploaded?\n ");
				successful = 0;
			}
			else {
				entry = GetFirst(&FileList);
				strcpy(name, entry->name);
			}
		}
	}
	else {
		if ((upfd = safeopen(name, WRITE_ANY)) == NULL) {
			mPrintf("\n Can't create %s!\n", name);
			successful = 0;
			return;
		}
		else {
#ifdef HORRID_AMIGA_LATTICE_BUG
			setnbf(upfd);
#endif
			successful = (Reception(Protocol, putFLChar) ==
							TRAN_SUCCESS);
			fclose(upfd);
		}
	}

	if (successful) {
		NetDeCompress(Compression, name);
		homeSpace();
		DropFile(1, TDirBuffer);
		CitSystem(TRUE, "%s", Reader->Cmd);
		homeSpace();		/* in case the translator moved us */
		if (OR_Result(useless)) {
			MakeDeCompressedFilename(msgs, "msgs.cit", TDirBuffer);
			if (AddNetMsgs(msgs, AddOffLineMsg, FALSE, -1, FALSE)==ERROR) {
				mPrintf("Could not open %s errno %d, upload aborted.\n ", name, errno);
			}
		}
	}

	homeSpace();
	RmTempFiles(TDirBuffer);
	KillList(&FileList);
}

void AddOffLineMsg()
{
	if (roomBuf.rbflags.SHARED && logBuf.lbflags.NET_PRIVS &&
					thisRoom != MAILROOM)
		if (!netInfo(FALSE)) return;
	SaveMessage(FALSE);
}

/*
 * OfflineUp()
 *
 * This function lists the offline readers support available for uploads.
 */
void OfflineUp(char *target)
{
	char *c;
	void ListReader();

	target[0] = 0;
	if (GetFirst(&OfflineReaderUp) == NULL)
		strcpy(target, "None.");
	else {
		RunListA(&OfflineReaderUp, ListReader, target);
		if ((c = strrchr(target, ',')) != NULL)
			strcpy(c, ".");
	}
}

/*
 * OfflineDown()
 *
 * This function lists the offline readers support available for downloads.
 */
void OfflineDown(char *target)
{
	char *c;
	void ListReader();

	target[0] = 0;
	if (GetFirst(&OfflineReaderDown) == NULL)
		strcpy(target, "None.");
	else {
		RunListA(&OfflineReaderDown, ListReader, target);
		if ((c = strrchr(target, ',')) != NULL)
			strcpy(c, ".");
	}
}

/*
 * ListReader()
 *
 * This function makes a readable version of an offline reader for display
 * by the help system.
 */
static void ListReader(OfflineReader *Reader, char *target)
{
	sprintf(lbyte(target), "<%c>%s, ", Reader->Selector,
			(Reader->Selector == Reader->Name[0]) ?
			Reader->Name + 1 : Reader->Name);
}

