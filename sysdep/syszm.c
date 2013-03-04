/*
 *				syszm.c
 *
 * External protocol handlers.
 */

/*
 *				history
 *
 * 89Aug14 HAW  Rewritten for versatility.
 * 88Nov13 HAW  Created.
 */

#define SYSTEM_DEPENDENT

#include "ctdl.h"

/*
 *				Contents
 *
 *	ReadSysProtInfo()	Read system info from ctdlprot.sys
 *	RunExternalProtocol()	Handles all external protocols
 *
 *		# == local for this implementation only
 */

extern CONFIG    cfg;
extern aRoom     roomBuf;
extern char      loggedIn;
extern int       SystemPort;
extern char	 onConsole;	/* Where we get stuff from      */
extern logBuffer logBuf;
extern int thisRoom;
extern UNS_32 BaudRate;

char AddStringToMCL(char *target, char *source, int len);

/*
 * RunExternalProtocol()
 *
 * This function handles external protocols.
 */
int RunExternalProtocol(PROTOCOL *Prot, char *mask)
{
    char     cmdline[125];	/* we have a limit */
    int      toReturn;

    if (!MakeCmdLine(cmdline, Prot->SysProtInfo->CmdLine, mask,
				sizeof cmdline - 1)) {
	if (strchr(mask, '>') != NULL || strchr(mask, '<') != NULL) {
	    mPrintf("Sorry, the list of files would be too long.  Try again without a date specification.\n ");
	    return TRAN_FAILURE;
	}
	else if (!MakeCmdLine(cmdline,Prot->SysProtInfo->CmdLine, mask,
					sizeof cmdline - 1)) {
	    mPrintf("Sorry, the list of files would be too long.\n ");
	    return TRAN_FAILURE;
	}
    }

    if (loggedIn) printf("\n(%s)\n", logBuf.lbname);

    CitSystem(TRUE, "%s", cmdline);

    toReturn = TRAN_SUCCESS;

    return toReturn;
}

UNS_32 intrates[] = { 30, 120, 240, 480, 960, 1440, 1920, 3840, 5680 };
/*
 * MakeCmdLine()
 *
 * This function creates a command line, including supported substitution
 * parameters.  This should be detailed in this comment but isn't.
 */
char MakeCmdLine(char *target, char *source, char *miscdata, int len)
{
    extern int BaudFlags;
    char *c, *temp, NoOverflow = TRUE;
    int  i;

    for (i = 0, c = source; *c; c++) {
	if (i > len - 2) {
	    NoOverflow = FALSE;
	    break;
	}
	if (*c == '%') {
	    target[i] = 0;
	    c++;
	    switch (*c) {
		case 'a':	/* baud rate	*/
		    if (onConsole)
			sprintf(lbyte(target), "%d", 0);
		    else
			sprintf(lbyte(target), "%ld",
			(cfg.DepData.LockPort != -1) ?
			 intrates[cfg.DepData.LockPort] * 10l : BaudRate);
		    break;
		case 'b':	/* bps		*/
		    sprintf(lbyte(target), "%ld",
			(cfg.DepData.LockPort != -1) ?
			 intrates[cfg.DepData.LockPort] : (BaudRate/10l));
		    break;
		case 'c':	/* port #	*/
		    sprintf(lbyte(target), "%d", SystemPort);
		    break;
		case 'g':	/* file mask	*/
		    NoOverflow = AddStringToMCL(target, miscdata, len);
		    break;
		case 'h':
		    if (BaudRate != 0l)
			sprintf(lbyte(target), "COM%d", SystemPort);
		    else
			strcat(target, "LOCAL");
		    break;
		case 'i':
		    NoOverflow = AddStringToMCL(target, roomBuf.rbname, len);
		    break;
		case 'j':
		    if ((temp = FindDirName(thisRoom)) != NULL)
			NoOverflow = AddStringToMCL(target, temp, len);
		    break;
		case 'k':
		    sprintf(lbyte(target), "%d", logBuf.lbwidth);
		    break;
		case 'l':
		    sprintf(lbyte(target), "%ld", BaudRate);
		    break;
		case 'm':
		    if (BaudFlags & MNP) 
			strcat(target, "MNP");
		    break;
		case 'd':
		    strcat(target, logBuf.lbname);
		    break;
	    }
	    while (target[i]) i++;
	}
	else target[i++] = *c;
    }
    target[i] = 0;
    return NoOverflow;
}

/*
 * AddStringToMCL()
 *
 * This adds a string as needed, I guess.
 */
char AddStringToMCL(char *target, char *source, int len)
{
    if (strlen(source) + strlen(target) < len - 2) {
	strcat(target, source);
	return TRUE;
    }
    else {
	strncpy(lbyte(target), source, len - strlen(target) - 1);
	target[len - 1] = 0;
	return FALSE;
    }
}

/*
 * ReadSysProtInfo()
 *
 * This function reads the special information for later use when running
 * external protocols.
 *
 *	[name] [flags] [selector] [system-dependent info]
 */
SystemProtocol *ReadSysProtInfo(char *original)
{
    int rover;
    char *c;
    SystemProtocol *data;

    for (rover = 0, c = original; rover < 4; rover++) {
	if ((c = strchr(c, ' ')) == NULL) return NULL;
	c++;
    }
    data = GetDynamic(sizeof *data);
    data->CmdLine = strdup(c);
    return data;
}

/*
 * ExternalTransfer()
 *
 * This function is used by the network for external protocol transfers.
 */
char ExternalTransfer(PROTOCOL *Prot, char *filename)
{
	char     *name, *fn;
	extern char *UploadLog;
	char cwd[250];

	getcwd(cwd, sizeof cwd);
	fn = strdup(filename);
	if ((name = strrchr(fn, '\\')) != NULL) {
		*name++ = 0;
		chdir(fn);
	}
	else {
		name = fn;
	}

	RunExternalProtocol(Prot, name);
	chdir(cwd);
	unlink(UploadLog);
	return TRUE;
}
