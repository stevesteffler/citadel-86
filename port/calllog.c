/*
 *				calllog.c
 *
 * handles call log of Citadel-86
 */

#include "ctdl.h"

/*
 *				history
 *
 * 92Jul07 HAW  Substantial revisions.
 * 88Sep02 HAW  Download/Upload log.
 * 86Mar07 HAW  New users and .ts signals.
 * 86Feb09 HAW  System up and down times.
 * 86Jan22 HAW  Set extern var so entire system knows baud.
 * 85Dec08 HAW  Put blank lines in file.
 * 85Nov?? HAW  Created.
 */

/*
 *				Contents
 *
 *	logMessage()		Put out log message to file.
 *	fileMessage()		Handles file messages.
 */

#ifdef NEEDED

struct timeData {
    int   year, lastuserday, day, hour, minute, curbaud;
    char  month[5];
    label person;
    int   flags;
};

#endif
struct timeData lgin;

extern CONFIG        cfg;
extern logBuffer     logBuf;
extern PROTO_TABLE   Table[];
extern aRoom         roomBuf;
extern UNS_32	     BaudRate;
extern MessageBuffer msgBuf;
extern char CallCrash;

static char CallFn[80] = "";	/* can't use SYS_FILE here I fear */

struct {
	int flag;
	char val;
} Flags[] = {
	{ LOG_NEWUSER,   '+' },
	{ LOG_CHATTED,   'C' },
	{ LOG_EVIL,      'E' },
	{ LOG_BADWORD,   'B' },
	{ LOG_TERM_STAY, '-' },
	{ LOG_TIMEOUT,   't' },
};

/*
 * logMessage()
 *
 * This function puts messages in the CALLLOG.SYS file depending on a number
 * of different events and conditions.
 */
void logMessage(int typemessage, UNS_32 val, int flags)
{
    static int oldDay = 0;
    FILE *fd;
    int  yr, dy, hr, mn, rover;
    char *mon, buf[100];
    char *format = "%s %s @ %d:%02d%s";
    char *SaveName = "callsave.sys";

    if (CallCrash) return;

    if (typemessage == BAUD || typemessage == DOOR_RETURN) {
	BaudRate = val;
    }

    if (cfg.Audit == 0) return;

    makeAuditName(CallFn, "calllog.sys");

    getCdate(&yr, &mon, &dy, &hr, &mn);
    switch (typemessage) {
	case SET_FLAG:
		lgin.flags |= flags;
		break;
	case FIRST_IN:
		oldDay = dy;
		sprintf(buf, format, "System brought up", formDate(), hr, mn,
									"");
		CallMsg(CallFn, buf);
		break;
#ifndef NO_DOORS
	case DOOR_RETURN:
		oldDay  = dy;
		if ((fd = fopen(SaveName, READ_ANY)) != NULL) {
		    fread(&lgin, sizeof lgin, 1, fd);
		    fclose(fd);
		    unlink(SaveName);
		}
		else printf("No luck with %s.", SaveName);
		break;
#endif
	case CRASH_OUT:
	case LAST_OUT:
		sprintf(buf, format, "System brought down", formDate(), hr, mn,
			(typemessage == CRASH_OUT) ? " (crash exit!)" : "");
		CallMsg(CallFn, buf);
		return;
#ifndef NO_DOORS
	case DOOR_OUT:
		if ((fd = fopen(SaveName, WRITE_ANY)) != NULL) {
		    fwrite(&lgin, sizeof lgin, 1, fd);
		    fclose(fd);
		}
		else printf("No luck with %s.", SaveName);
		break;
#endif
	case BAUD:
		lgin.person[0] = 0;
		lgin.flags = 0;
		goto datestuff;		/* ACK!  ACK!  ACK! */
	case L_IN: 
		strCpy(lgin.person, logBuf.lbname);
		lgin.flags = flags;
	case INTO_NET:
datestuff:
		lgin.year = yr;
		lgin.day = dy;
		lgin.hour = hr;
		lgin.minute = mn;
		strcpy(lgin.month, mon);
		break;
	case CARRLOSS:
		homeSpace();	/* back to our regular lair, don't break! */
	case L_OUT:
		lgin.flags |= flags;
		/*
		 * BaudRate > 0 means the user is on the system console.
		 * So this code means "If no person is logged in and
		 * Anonymous session logging is off OR the anonymous user
		 * was at the system console ..."
		 */
		if (!lgin.person[0] &&
			!(cfg.BoolFlags.AnonSessions && BaudRate > 0)) {
		    break;
		}

		if (lgin.lastuserday != dy && lgin.lastuserday != 0)
		    CallMsg(CallFn, "");

		lgin.lastuserday = dy;

		sprintf(buf,
			"%-22s: %2d%s%02d %2d:%02d - %2d:%02d ",
			(strLen(lgin.person)) ? lgin.person : "<No Login>",
			lgin.year, lgin.month, lgin.day, lgin.hour, lgin.minute,
			hr, mn);

		switch (BaudRate) {
		case (UNS_32) 0:
			strcat(buf, "(sysConsole)");
			break;
		case (UNS_32) -1:
			strcat(buf, "(Unknown)");
			break;
		default:
			sprintf(lbyte(buf), "(%ld)", BaudRate);
		}

		for (rover = 0; rover < NumElems(Flags); rover++) {
			if (lgin.flags & Flags[rover].flag)
				sprintf(lbyte(buf), " %c", Flags[rover].val);
		}

		CallMsg(CallFn, buf);

		lgin.person[0] = 0;
		oldDay = dy;
		if (typemessage == CARRLOSS) BaudRate = 0l;
		goto datestuff;
	case OUTOF_NET:
		if (cfg.Audit == 1) {
		    sprintf(buf,
			"System in network mode: %d%s%02d %2d:%02d - %2d:%02d",
			lgin.year, lgin.month, lgin.day, lgin.hour,
			lgin.minute, hr, mn);
		    CallMsg(CallFn, buf);
		}
		break;

	default: printf("crashout: unknown case in switch statement");
    }
}

/*
 * fileMessage()
 *
 * This function handles the upload/download file log.
 */
void fileMessage(char mode, char *fn, char IsDL, int protocol, long size)
{
    long	  hours, mins;
    static label  LastActive = "";
    char	  logfn[100];
    /* int	 	  yr, dy, hr, mn; */
    char	  *mon, *pr, buf[100];
    static struct timeData gData, fData, xData;
    struct timeData *pData;
    static long xwork;
    static char fin_done;

    if (protocol == ASCII || cfg.Audit == 0)
	return;

    makeAuditName(logfn, "filelog.sys");
/*  getCdate(&yr, &mon, &dy, &hr, &mn); */
    getCdate(&gData.year, &mon, &gData.day, &gData.hour, &gData.minute);
    strcpy(gData.month, mon);

    switch (mode) {
    case FL_START:
	startTimer(USER_TIMER);
	fin_done = FALSE;
	fData = gData;
	break;
    case FL_FIN:
	fin_done = TRUE;
	xData = gData;
	xwork = chkTimeSince(USER_TIMER);
	break;
    case FL_SUCCESS:
    case FL_FAIL:
    case FL_EX_END:
	if (strCmpU(LastActive, logBuf.lbname) != SAMESTRING) {
	    sprintf(buf, "\n%s on %d%s%02d @ ", logBuf.lbname, gData.year, mon,
								gData.day);
	    if (BaudRate > 0l) 
		sprintf(lbyte(buf), "%ld", BaudRate);
	    else
		strcat(buf, "Unknown");

	    strcat(buf, ":");

	    CallMsg(logfn, buf);
	    strCpy(LastActive, logBuf.lbname);
	}
	if (!fin_done) {
	    xwork = chkTimeSince(USER_TIMER);
	    pData = &gData;
	}
	else
	    pData = &xData;
	hours = xwork / 3600;
	xwork -= (hours * 3600);
	mins  = xwork / 60;
	xwork -= (mins * 60);
	if (protocol > TOP_PROTOCOL)
	    pr = FindProtoName(protocol);
	else
	    pr = Table[protocol].GenericName;
	if (mode == FL_EX_END) {
	    sprintf(buf,
    "%2cFollowing files %c %s via %s %d:%02d - %d:%02d (%ld:%02ld:%02ld):",
		' ', (IsDL) ? 'D' : 'U', roomBuf.rbname, 
		pr,
		fData.hour, fData.minute, pData->hour, pData->minute,
		hours, mins, xwork);
	    CallMsg(logfn, buf);
	    CallMsg(logfn, msgBuf.mbtext);
	}
	else {
	    if (!IsDL && mode == FL_FAIL)
		sprintf(buf, "%2c%s (FAILED) %c %s: %d:%02d - %d:%02d (%ld:%02ld:%02ld) %s.",
		' ', fn, (IsDL) ? 'D' : 'U', roomBuf.rbname,
		fData.hour, fData.minute, pData->hour, pData->minute,
		hours, mins, xwork, pr);
	    else
		sprintf(buf,
    		"%2c%s (%ld) %c %s: %d:%02d - %d:%02d (%ld:%02ld:%02ld) %s. %s",
		' ', fn, size, (IsDL) ? 'D' : 'U', roomBuf.rbname,
		fData.hour, fData.minute, pData->hour, pData->minute,
		hours, mins, xwork, pr,
		(mode == FL_FAIL) ? "(FAILED)" : "");
	    CallMsg(logfn, buf);
	}
	break;
    }
}
