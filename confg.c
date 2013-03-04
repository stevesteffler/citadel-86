/*
 *			      confg.c
 *
 * Configuration program for Citadel bulletin board system.
 */

#define CONFIGURE

#include "ctdl.h"

/*
 *				History
 *
 * 85Dec26 HAW  Add CALL-LOG define.
 * 85Nov15 HAW  MS-DOS library update.
 * 85Oct27 HAW  Kill CERMETEK.
 * 85Oct17 HAW  Add paramVers, change bauds to array, searchBaud chg.
 * 85Oct16 HAW  Kill CLOCK, add officeStuff.
 * 85Aug24 HAW  Add duomessage file, NETDISK specification.
 * 85Jul07 HAW  Update so won't go through total recon. on init.
 * 85May27 HAW  Start stuffing in auto-networking stuff.
 * 85May22 HAW  Start conversion to make log file size sysop selectable.
 * 85May11 HAW  Make "Lobby" sysop definable
 * 85May06 HAW  Add bail out code.
 * 85May05 HAW  Add helpDisk parameter for 3 disk system.
 * 85Apr10 HAW  Fix logSort, alphabetize file.
 * 85Mar11 HAW  Put all user functions in this file.
 * 85Feb18 HAW  Add baud search stuff.
 * 85Jan20 HAW  Use MSDOS #define for date stuff.
 * 84Sep05 HAW  Isolate strangeness in compiler's library.  See note.
 * 84Aug30 HAW  Onwards to MS-DOS!
 * 84Apr08 HAW  Update to BDS C 1.50a begun.
 * 82Nov20 CrT  Created.
 */

/*
 *				Contents
 *
 *	dGetWord()		reads a word off disk
 *	init()			system startup initialization
 *	main()			main controller
 *	illegal()		abort bottleneck
 *	msgInit()		sets up cfg.catChar, catSect etc.
 *	zapMsgFile()		initialize ctdlmsg.sys
 *	realZap()		does work of zapMsgFile()
 *	indexRooms()		build RAM index to ctdlroom.sys
 *	noteRoom()		enter room into RAM index	
 *	zapRoomFile()		erase & re-initialize ctdlroom.sys
 *	hash()			hashes a string to an integer
 *	logInit()		builds the RAM index to CTDLLOG.SYS
 *	noteLog()		enters a userlog record into RAM index
 *	sortLog()		sort CTDLLOG by time since last call
 *	wrapup()		finishes and writes ctdlTabl.sys
 *	zapLogFile()		erases & re-initializes CTDLLOG.SYS
 */

/*
 *		Strangenesses   (Hue, Jr., 12Sep84)
 *	Have discovered that the line:
 *	sscanf(line, "\"%s\"", str);
 *	is not parsed the same way by this compiler as it is by BDS;
 *	this is highly unfortunate and excrable.  So, all porters
 *	should note that scanf() is not, in any way, "portable."  If
 *	BDS is "non-standard", then the standard sucks.
 */
#define BAUDS	\
"Valid SYSBAUD values:\n\
0=300\n\
1=3/12\n\
2=../24\n\
3=../48\n\
4=../96\n\
5=../14.4\n\
6=../19.2\n\
7=../38.4\n\
8=../56.8\n"


struct GenList {
    char *GenName;
    int  GenVal;
} ;

extern FILE *msgfl, *msgfl2;    /* file descriptor for the msg file	*/
label	OldBaseRoom;
char	 *baseRoom;
int	  mailCount=0;
char	 msgZap =  FALSE,
	     logZap =  FALSE,
	     roomZap = FALSE;

char  FirstInit = FALSE;
char  ReInit = FALSE;

static DATA_BLOCK sectBuf;

long FloorSize;
int  DefaultPrefix;

void RedirectProcess();
char NetParse(char *line, int *offset, char *var, int arg);

extern SListBase Events;		/* event list			*/
extern SListBase ProcessLater;		/* event list			*/
extern CONFIG    cfg;		   /* The configuration variable	*/
extern MessageBuffer   msgBuf;		/* The -sole- message buffer	*/
extern NetBuffer netBuf;
extern rTable    *roomTab;	      /* RAM index of rooms		*/
extern aRoom     roomBuf;	       /* room buffer			*/
extern EVENT     *EventTab;
extern int       thisRoom;	      /* room currently in roomBuf	*/
extern int       thisNet;
extern LogTable  *logTab;	       /* RAM index of pippuls		*/
extern logBuffer logBuf;		/* Log buffer of a person       */
extern SListBase Serves;
extern FILE      *logfl;		/* log file descriptor		*/
extern FILE      *roomfl;	       /* file descriptor for rooms	*/
extern FILE      *netfl;
extern LogTable  *logTab;	       /* RAM index of pippuls		*/
extern int       thisLog;	       /* entry currently in logBuf	*/
extern NetTable  *netTab;
extern char	   *R_SH_MARK, *LOC_NET, *NON_LOC_NET;
extern char   *W_R_ANY;
extern char   *R_W_ANY;
extern char   *READ_ANY;
extern char   *WRITE_ANY;

void CheckNet( void );
void TheAreaCheck( void );
int ReadDialOut(char *line, int baud, int *offset);

/*
 * init()
 *
 * The master system initialization.
 */
void init(int attended)
{
    unsigned char c;
    SYS_FILE      tempName;
    extern int    errno, _doserrno;

    cfg.BoolFlags.noChat      = TRUE;

    /* initialize input character-translation table:	*/
    for (c = 0;  c < '\40';  c++) {
	cfg.filter[c] = '\0';	   /* control chars -> nulls	*/
    }
    cfg.filter[1] = 'y';	/* Gallifrey Gal fix */
    cfg.filter[7] = 7;

    for (c='\40'; c < 128;   c++) {
	cfg.filter[c] = c;	      /* pass printing chars	*/
    }
    cfg.filter[SPECIAL]     = SPECIAL;
    cfg.filter[CNTRLl]      = CNTRLl;
    cfg.filter[DEL      ]   = BACKSPACE;
    cfg.filter[BACKSPACE]   = BACKSPACE;
    cfg.filter[XOFF     ]   = XOFF     ;
    cfg.filter[XON      ]   = XON      ;
    cfg.filter['\r'     ]   = NEWLINE  ;
    cfg.filter[CNTRLO   ]   = 'N'      ;

    mvToHomeDisk(&cfg.homeArea);

    makeSysName(tempName, "ctdlmsg.sys",  &cfg.msgArea);
    if ((msgfl = fopen(tempName, R_W_ANY)) == NULL) {
	if (!attended)
	    illegal("!System must be attended for creation!");
	printf(" %s not found, creating new file. \n", tempName);
	if ((msgfl = fopen(tempName, W_R_ANY)) == NULL)
	    illegal("?Can't create the message file!");
	printf(" (Be sure to initialize it!)\n");
    }

    makeSysName(tempName, "ctdllog.sys", &cfg.logArea);
    /* open userlog file */
    if ((logfl = fopen(tempName, R_W_ANY)) == NULL) {
	if (!attended)
	    illegal("!System must be attended for creation!");
	printf(" %s not found, creating new file. \n", tempName);
	if ((logfl = fopen(tempName, W_R_ANY)) == NULL)
	    illegal("?Can't create log file!");
	printf(" (Be sure to initialize it!)\n");
    }

    makeSysName(tempName, "ctdlroom.sys", &cfg.roomArea);
    /* open room file */
    if ((roomfl = fopen(tempName, R_W_ANY)) == NULL) {
	if (!attended)
	    illegal("!System must be attended for creation!");
	printf(" %s not found, creating new file. \n", tempName);
	if ((roomfl = fopen(tempName, W_R_ANY)) == NULL)
	   illegal("?Can't create room file!");
	printf(" (Be sure to initialize it!)\n");
    }
    else CheckBaseroom();	/* ugly hack */

    if (cfg.BoolFlags.netParticipant) {
	makeSysName(tempName, "ctdlnet.sys", &cfg.netArea);
	if ((netfl = fopen(tempName, READ_ANY)) == NULL) {
	    printf(" %s not found, creating new file.\n", tempName);
	    if ((netfl = fopen(tempName, WRITE_ANY)) == NULL)
		illegal("?Can't create the net file!");
	}
    }

    CheckFloors();

    if (attended || FirstInit) {
	printf("\n Erase and initialize log, message and/or room files?");
	if (FirstInit || toUpper(simpleGetch()) == 'Y') {
	    /* each of these has an additional go/no-go interrogation: */
	    msgZap  = zapMsgFile();
	    roomZap = zapRoomFile();
	    logZap  = zapLogFile();
	}
    }
}

static int  necessary[13]   = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#define HELP		0
#define LOG		1
#define ROOM		2
#define MSG		3
#define MSG2		4
#define NET_STUFF	5
#define CALL		6
#define HOLD		7
#define FLOORA		8
#define DOM_STUFF	9
#define INFO_STUFF	10
#define BIOA		11
/*
 * main()
 *
 * Main manager for confg.c.
 */
main(int argc, char **argv)
{
    FILE *fBuf, *pwdfl;
    char line[90], status, *strchr(), *g;
    char onlyParams = FALSE, CleanCalllog;
    char var[90];
    int  arg;
    int  i, offset = 1;
    extern char *READ_TEXT;
    int  SetVal;

    if (argc > 1) {
	if (strCmpU(argv[1], "exists") == 0) {
	    fBuf = fopen("exists", "w");
	    fprintf(fBuf, "I exist.");
	    fclose(fBuf);
	    exit(1);
	}
    }

    cfg.paramVers = 12;

    printf("%s Configurator (V%d.11)\n%s\n\n", VARIANT_NAME,
					cfg.paramVers, COPYRIGHT);

    if (access(LOCKFILE, 0) != -1) {
	printf(
"You are apparently reconfiguring from within Citadel, which is a No No!\n"
"Do you wish to continue? ");
	if (toUpper(simpleGetch()) != 'Y')
	    exit(7);
	unlink(LOCKFILE);
    }

    zero_struct(cfg);

    for (i = 1;  i < argc; i++) {
	if (strCmpU(argv[i], "onlyParams") == SAMESTRING)
	    onlyParams = TRUE;
	else if (strCmpU(argv[i], "FirstInit") == SAMESTRING)
	    FirstInit = TRUE;
	else if (strCmpU(argv[i], "ReInit") == SAMESTRING)
	    ReInit = TRUE;
	else if (!sysArgs(argv[i]))
	    ReInit = TRUE;
    }

    cfg.weAre     = CONFIGUR;
    if (!(SetVal = readSysTab(FALSE, FALSE))) {
	cfg.sysPassword[0] = 0;
	cfg.MAXLOGTAB = 0;      /* Initialize, just in case		*/
	cfg.ConTimeOut = -1;	/* optional parameter	*/
	if (onlyParams) {
	    printf("'onlyParams' parameter ignored\n");
	    onlyParams = FALSE;
	}
    }
    else {
	if (EventTab != NULL) free(EventTab);
	i = cfg.BoolFlags.noChat;
	zero_struct(cfg.BoolFlags);
	cfg.BoolFlags.noChat = i;
	strcpy(OldBaseRoom, &cfg.codeBuf[offset]);
    }

    cfg.paramVers = 12;		/* yes, set it twice* 	*/
    cfg.weAre     = CONFIGUR;	/* same */

    InitBuffers();	      /* initializes message buffers */

    initSysSpec();	      /* Call implementation specific code	*/

				/* these are mostly optional param values */
    cfg.SysopName[0]     = 0;
    cfg.SysopArchive[0]  = 0;
    strcpy(cfg.DomainDisplay, " _ %s");
    cfg.InitColumns      = 40;
    cfg.EvNumber	 = 0;
    cfg.MailHub 	 = 0;
    cfg.DomainHandlers	 = 0;
    cfg.sizeArea	 = 0;
    cfg.maxFileSize	 = 0;
    KillList(&Serves);
    CleanCalllog	 = FALSE;
    cfg.Audit	    = 0;
    cfg.BoolFlags.DL_Default = TRUE;
    cfg.AnonMailLength   = 0;
    cfg.LoginAttempts    = 0;
    zero_array(cfg.DialPrefixes);
    cfg.LD_Delay = 60;
    cfg.ParanoiaLimit = 100;
    cfg.MaxNotStable = 100;
    cfg.ArchiveWidth = 80;
    cfg.BoolFlags.NoInfo = TRUE;
    cfg.BoolFlags.NoMeet = TRUE;

    EventTab = NULL;
    if ((fBuf = fopen("ctdlcnfg.sys", READ_TEXT)) == NULL) {/* ASCII mode   */
	printf("?Can't find ctdlCnfg.sys!\n");
	exit(1);
    }

    while (fgets(line, 90, fBuf) != NULL) {
	if (line[0] != '#') continue;
	if (sscanf(line, "%s %d ", var, &arg)) {
	    printf("%s", line);
	    if (NetParse(line, &offset, var, arg)) {
		continue;
	    } else if (strcmp(var, "#CRYPTSEED" )    == SAMESTRING) {
		cfg.cryptSeed   = arg;
	    } else if (strcmp(var, "#ECD-DEFAULT" )    == SAMESTRING) {
		cfg.ECD_Default   = arg;
	    } else if (strcmp(var, "#MESSAGEK"  )    == SAMESTRING) {
		cfg.maxMSector  = arg*(1024/MSG_SECT_SIZE);
	    } else if (strcmp(var, "#LOGINOK"   )    == SAMESTRING) {
		cfg.BoolFlags.unlogLoginOk= arg;
	    } else if (strcmp(var, "#ISDOOR"   )    == SAMESTRING) {
		cfg.BoolFlags.IsDoor = TRUE;
	    } else if (strcmp(var, "#DoorPrivs")     == SAMESTRING) {
		cfg.BoolFlags.DoorDft     = arg;
	    } else if (strcmp(var, "#FILE-PRIV-DEFAULT")     == SAMESTRING) {
		cfg.BoolFlags.DL_Default     = arg;
	    } else if (strcmp(var, "#ENTEROK"   )    == SAMESTRING) {
		cfg.BoolFlags.unlogEnterOk= arg;
	    } else if (strcmp(var, "#READOK"    )    == SAMESTRING) {
		cfg.BoolFlags.unlogReadOk = arg;
	    } else if (strcmp(var, "#ROOMOK"    )    == SAMESTRING) {
		cfg.BoolFlags.nonAideRoomOk=arg;
	    } else if (strcmp(var, "#ALLMAIL"   )    == SAMESTRING) {
		cfg.BoolFlags.noMail      = !arg;
	    } else if (strcmp(var, "#AUTO-MODERATORS"   )    == SAMESTRING) {
		cfg.BoolFlags.AutoModerate = arg;
	    } else if (strcmp(var, "#UNLOGGED-WIDTH") == SAMESTRING) {
		cfg.InitColumns = arg;
	    } else if (strcmp(var, "#LOGIN-ATTEMPTS") == SAMESTRING) {
		cfg.LoginAttempts = arg;
	    } else if (strcmp(var, "#ANON-MAIL-LENGTH") == SAMESTRING) {
		cfg.AnonMailLength = arg;
	    } else if (strcmp(var, "#MAX-MSGS-ENTERED") == SAMESTRING) {
		cfg.ParanoiaLimit = arg;
	    } else if (strcmp(var, "#DISK-FREE") == SAMESTRING) {
		cfg.LowFree = arg;
	    } else if (strcmp(var, "#CLEAN-CALLLOG") == SAMESTRING) {
		CleanCalllog = TRUE;
	    } else if (strcmp(var, "#NO-CONSOLE-BANNER") == SAMESTRING) {
		cfg.BoolFlags.NoConBanner = TRUE;
	    } else if (strcmp(var, "#PARANOID-LOGIN") == SAMESTRING) {
		cfg.BoolFlags.ParanoidLogin = TRUE;
	    } else if (strcmp(var, "#ANONYMOUS-SESSIONS") == SAMESTRING) {
		cfg.BoolFlags.AnonSessions = TRUE;
	    } else if (strcmp(var, "#LOGSIZE"   )    == SAMESTRING) {
		if (SetVal) {
		    if (cfg.MAXLOGTAB != arg)
		       illegal(
			 "LOGSIZE parameter does not equal old value!");
		}
		else {
		    cfg.MAXLOGTAB   = arg;
		    logTab = (LogTable *)
			   GetDynamic(sizeof(*logTab) * arg);
		}
	    } else if (strcmp(var, "#MAXROOMS"  )    == SAMESTRING) {
		if (SetVal) {
		    if (MAXROOMS != arg)
			illegal(
			  "MAXROOMS parameter does not equal old value!");
		}
		else {
		    MAXROOMS = arg;
		    if (MAXROOMS <= 3)
			illegal("MAXROOMS must be greater than 3!");
		    roomTab = (rTable *)
				   GetDynamic(MAXROOMS * sizeof *roomTab);
		}
	    } else if (strcmp(var, "#MSG-SLOTS" )    == SAMESTRING) {
		if (SetVal) {
		    if (MSGSPERRM != arg)
			illegal(
			  "MSGSPERRM parameter does not equal old value!");
		}
		else {
		    MSGSPERRM = arg;
		}
	    } else if (strcmp(var, "#MAIL-SLOTS")    == SAMESTRING) {
		if (SetVal) {
		    if (MAILSLOTS != arg)
			illegal(
			  "MAILSLOTS parameter does not equal old value!");
		}
		else {
		    MAILSLOTS = arg;
		}
	    } else if (strcmp(var, "#AIDESEEALL")    == SAMESTRING) {
		cfg.BoolFlags.aideSeeAll = arg;
	    } else if (strcmp(var, "#CONSOLE-TIMEOUT")    == SAMESTRING) {
		cfg.ConTimeOut = arg;
	    } else if (strcmp(var, "#ARCHIVE-WIDTH")    == SAMESTRING) {
		cfg.ArchiveWidth = arg;
	    } else if (strcmp(var, "#SYSBAUD"   )    == SAMESTRING) {
		cfg.sysBaud   = arg;
		if (arg > 8 || arg < 0) {
		    illegal(BAUDS);
		}
	    } else if (strcmp(var, "#event"    ) == SAMESTRING) {
		offset = EatEvent(line, offset);
	    } else if (strcmp(var, "#nodeTitle") == SAMESTRING) {
		readString(line, &cfg.codeBuf[offset], TRUE);
		cfg.nodeTitle = offset;
		while (cfg.codeBuf[offset])
		    offset++;
		offset++;
	    } else if (strcmp(var, "#sysPassword") == SAMESTRING) {
		    readString(line, cfg.sysPassword, FALSE);
		    if ((pwdfl = fopen(cfg.sysPassword, READ_TEXT)) == NULL) {
			printf("\nNo system password file found\n");
			cfg.sysPassword[0] = 0;
		    }
		    else {
			fgets(cfg.sysPassword, 199, pwdfl);
		      /*  cfg.sysPassword[strLen(cfg.sysPassword) - 1] = 0;*/
			while ((g = strchr(cfg.sysPassword, '\n')) != NULL)
			    *g = 0;
			if (strLen(cfg.sysPassword) < 15) {
			  printf("\nSystem password is too short -- ignored\n");
			    cfg.sysPassword[0] = 0;
			}
			fclose(pwdfl);
		    }

	    } else if (strcmp(var, "#sysopName") == SAMESTRING) {
		    readString(line, msgBuf.mbtext, FALSE);
		    if (strLen(msgBuf.mbtext) > 19)
			illegal("SysopName too long; must be less than 20");
		    strcpy(cfg.SysopName, msgBuf.mbtext);
	    } else if (strcmp(var, "#SYSOP-ARCHIVE") == SAMESTRING) {
		    readString(line, msgBuf.mbtext, FALSE);
		    if (strLen(msgBuf.mbtext) > sizeof cfg.SysopArchive) {
			sprintf(msgBuf.mbtext, "SysopName too long; must be less than %d", sizeof cfg.SysopArchive);
			illegal(msgBuf.mbtext);
		    }
		    strcpy(cfg.SysopArchive, msgBuf.mbtext);
	    } else if (strcmp(var, "#baseRoom") == SAMESTRING) {
		    readString(line, &cfg.codeBuf[offset], TRUE);
		    if (strLen(&cfg.codeBuf[offset]) > 19)
			illegal("baseRoom too long; must be less than 20");
		    cfg.bRoom = offset;
		    baseRoom = &cfg.codeBuf[offset];
		    while (cfg.codeBuf[offset])
			 offset++;
		    offset++;
	    } else if (strcmp(var, "#MainFloor") == SAMESTRING) {
		    readString(line, &cfg.codeBuf[offset], TRUE);
		    if (strLen(&cfg.codeBuf[offset]) > 19)
			illegal("#MainFloor too long; must be less than 20");
		    cfg.MainFloor = offset;
		    while (cfg.codeBuf[offset])
			 offset++;
		    offset++;
	    } else if (strcmp(var, "#HELPAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.homeArea, offset);
		necessary[HELP]++;
	    } else if (strcmp(var, "#LOGAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.logArea, offset);
		necessary[LOG]++;
	    } else if (strcmp(var, "#ROOMAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.roomArea, offset);
		necessary[ROOM]++;
	    } else if (strcmp(var, "#MSGAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.msgArea, offset);
		necessary[MSG]++;
	    } else if (strcmp(var, "#MSG2AREA" )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.msg2Area, offset);
		cfg.BoolFlags.mirror = TRUE;
		necessary[MSG2]++;
	    } else if (strcmp(var, "#NETAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.netArea, offset);
		necessary[NET_STUFF]++;
		MakeNetCache(line);
		if (access(line, 0) != 0) mkdir(line);
	    } else if (strcmp(var, "#INFOAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.infoArea, offset);
		cfg.BoolFlags.NoInfo = FALSE;
		necessary[INFO_STUFF]++;
	    } else if (strcmp(var, "#DOMAINAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.domainArea, offset);
		necessary[DOM_STUFF]++;
	    } else if (strcmp(var, "#AUDITAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.auditArea, offset);
		necessary[CALL]++;
		cfg.Audit = 1;
	    } else if (strcmp(var, "#HOLDAREA"  )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.holdArea, offset);
		necessary[HOLD]++;
		cfg.BoolFlags.HoldOnLost = TRUE;
	    } else if (strcmp(var, "#FLOORAREA" )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.floorArea, offset);
		necessary[FLOORA]++;
	    } else if (strcmp(var, "#BIOAREA" )    == SAMESTRING) {
		offset = doArea(var, line, &cfg.bioArea, offset);
		necessary[BIOA]++;
		cfg.BoolFlags.NoMeet = FALSE;
	    } else if (strcmp(var, "#alldone") == SAMESTRING) {
		break;
	    } else {
		offset = sysSpecs(line, offset, &status, fBuf);
		if (!status)
		    printf("? -- no variable '%s' known! -- ignored.\n", var);
	    }
	}
    }

    for (i = 0; i <= B_7; i++)
	if (cfg.DialPrefixes[i] == 0) {
	    cfg.DialPrefixes[i] = DefaultPrefix;
	}

    if (cfg.Audit == 1)
	if (CleanCalllog) cfg.Audit = 2;

    initLogBuf(&logBuf);
    initRoomBuf(&roomBuf);
    initNetBuf(&netBuf);

    TheAreaCheck();

    if (!SysDepIntegrity(&offset))
	exit(2);

    RunListA(&ProcessLater, RedirectProcess, &offset);
    RunList(&Events, EvIsDoor);

    printf("offset=%d\n", offset);

    if (offset < MAXCODE) {
	initSharedRooms(TRUE);
	UpdateSharedRooms();
	if (!onlyParams) init(!ReInit);
	else {
	    CheckBaseroom();	/* ugly hack */
	    CheckFloors();
	    if (cfg.BoolFlags.netParticipant)
		CheckNet();
	}
	wrapup(onlyParams);
    } else {
	illegal(
"\7codeBuf[] overflow! Recompile with larger MAXCODE or reduce ctdlCnfg.sys\7"
	);
    }
    return 0;
}

/*
 * readString()
 *
 * This function reads a '#<id> "<value>"  since scanf can't.
 */
void readString(char *source, char *destination, char doProc)
{
    char string[300], last = 0;
    int  i, j;

    for (i = 0; source[i] != '"' && source[i]; i++)
	;

    if (!source[i]) {
	sprintf(string, "Couldn't find beginning \" in -%s-", source);
	illegal(string);
    }

    for (j = 0, i++; source[i] && 
		    (source[i] != '"' || (doProc && last == '\\'));
					     i++, j++) {
	string[j] = source[i];
	last = source[i];
    }
    if (!source[i]) {
	sprintf(string, "Couldn't find ending \" in -%s-", source);
	illegal(string);
    }
    string[j] = '\0';
    strcpy(destination, string);
    if (doProc) xlatfmt(destination);
}

/*
 * isoctal() * xlatfmt()
 *
 * contributed by Dale Schumacher, allow embedding of formatting info a la'
 * "C" style: \n, \t, etc....
 */
char isoctal( int c )
{
	return(( c >= '0' ) && ( c <= '7' ));
}

void xlatfmt( char *s )
{
	register char *p, *q;
	register int i;

	for( p=q=s; *q; ++q ) {
		if ( *q == '\\' )
			switch( *++q ) {
				case 'n' :
					*p++ = '\n';
					break;
				case 't' :
					*p++ = '\t';
					break;
				case 'r' :
					*p++ = '\r';
					break;
				case 'f' :
					*p++ = '\f';
					break;
				default :
					if ( isoctal( *q )) {
						i = (( *q++ ) - '0' );
						if ( isoctal( *q )) {
							i <<= 3;
							i += (( *q++ ) - '0' );
							if ( isoctal( *q )) {
								i <<= 3;
								i += (*q -'0');
							}
							else
							     --q;
						}
						else
							--q;
						*p++ = 0xFF & ((char) i);
					}
					else
						*p++ = *q;
					break;
			}
		else
			*p++ = *q;
	}
	*p = '\0';

}

/*
 * illegal()
 *
 * This will print out configure error message and aborts.
 */
void illegal(char *errorstring)
{
    printf("\007\nERROR IN CONFIGURATION:\n%s\nABORTING", errorstring);
    exit(7);
}

/*
 * dGetWord()
 *
 * This fetches one word from current message, off disk, returns TRUE if more
 * words follow, else FALSE.
 */
char dGetWord(char *dest, int lim)
{
    char c;

    --lim;      /* play it safe */

    /* pick up any leading blanks: */
    for (c = getMsgChar();   c == ' '  &&  c && lim;   c = getMsgChar()) {
	if (lim) { *dest++ = c;   lim--; }
    }

    /* step through word: */
    for (		;  c != ' ' && c && lim;   c = getMsgChar()) {
	if (lim) { *dest++ = c;   lim--; }
    }

    /* trailing blanks: */
    for (		;   c == ' ' && c && lim;   c = getMsgChar()) {
	if (lim) { *dest++ = c;   lim--; }
    }

    if (c)  unGetMsgChar(c);    /* took one too many    */

    *dest = '\0';	       /* tie off string       */

    return  c;
}

/*
 * msgInit()
 *
 * This sets up lowId, highId, cfg.catSector and cfg.catChar, by scanning over
 * ctdlmsg.sys.
 */
void msgInit()
{
    MSG_NUMBER first, here;
    CheckPoint Cpt;
    FILE *fd;
    extern struct mBuf mFile1;

    if ((fd = fopen(CHECKPT, READ_ANY)) != NULL) {
	fread(&Cpt, sizeof Cpt, 1, fd);
	fclose(fd);
	if (findMessage(Cpt.loc, Cpt.ltnewest, TRUE)) {
	    do {
		printf("message# %s\n", msgBuf.mbId);
		while (dGetWord(msgBuf.mbtext, MAXTEXT));
		cfg.catSector   = mFile1.thisSector;
		cfg.catChar     = mFile1.thisChar;
		cfg.newest      = atol(msgBuf.mbId);
		getMessage(getMsgChar, FALSE, FALSE, TRUE);
	    } while (atol(msgBuf.mbId) >= Cpt.ltnewest);
	    cfg.oldest = atol(msgBuf.mbId);
	    printf("\n\noldest=%lu\n", cfg.oldest);
	    printf("newest=%lu\n", cfg.newest);
	    return;
	}
    }

    startAt(msgfl, &mFile1, 0, 0);
    getMessage(getMsgChar, FALSE, FALSE, TRUE);

    /* get the ID# */
    first = atol(msgBuf.mbId);
    printf("message# %lu\n", first);

    cfg.newest = cfg.oldest = first;

    cfg.catSector   = mFile1.thisSector;
    cfg.catChar     = mFile1.thisChar;

    for (getMessage(getMsgChar, FALSE, FALSE, TRUE);
	     here = atol(msgBuf.mbId), here != first;
		      getMessage(getMsgChar, FALSE, FALSE, TRUE)) {
	printf("message# %lu\r", here);

	if (strCmpU("Mail", msgBuf.mbroom) == 0) mailCount++;

	/* find highest and lowest message IDs: */
	if (here < cfg.oldest && here != 0l) {
	    cfg.oldest = here;
	}
	if (here > cfg.newest) {
	    cfg.newest = here;

	    /* read rest of message in and remember where it ends,	*/
	    /* in case it turns out to be the last message		*/
	    /* in which case, that's where to start writing next message*/
	    while (dGetWord(msgBuf.mbtext, MAXTEXT));
	    cfg.catSector   = mFile1.thisSector;
	    cfg.catChar     = mFile1.thisChar;
	}
    }
    printf("\n\noldest=%lu\n", cfg.oldest);
    printf("newest=%lu\n", cfg.newest);
}

/*
 * zapMsgFile()
 *
 * This function initializes ctdlmsg.sys.
 */
char zapMsgFile()
{
    extern char *W_R_ANY;
    char fn[80];

    if (!FirstInit) {
	printf("\nDestroy all current messages? ");
	if (toUpper(simpleGetch()) != 'Y')   return FALSE;
    }

    if (cfg.BoolFlags.mirror) printf("Creating primary message file.\n");
    realZap();
    if (cfg.BoolFlags.mirror) {
	fclose(msgfl);
	makeSysName(fn, "ctdlmsg.sys", &cfg.msg2Area);
	if ((msgfl = fopen(fn, W_R_ANY)) == NULL)
	    illegal("?Can't create the secondary message file!");
	printf("Creating secondary message file.\n");
	realZap();
    }
    return TRUE;
}

/*
 * realZap()
 *
 * This does the work of zapMsgFile.
 */
char realZap()
{
    int   i;
    unsigned sect;

    /* put null message in first sector... */
    sectBuf[0]  = 0xFF; /*   \				*/
    sectBuf[1]  =  '1'; /*    >  Message ID "1" MS-DOS style  */
    sectBuf[2]  = '\0'; /*   /				*/
    sectBuf[3]  =  'M'; /*   \    Null messsage	       */
    sectBuf[4]  = '\0'; /*   /				*/

    cfg.newest = cfg.oldest = 1l;

    cfg.catSector   = 0;
    cfg.catChar     = 5;

    for (i=5;  i<MSG_SECT_SIZE;  i++) sectBuf[i] = 0;

    crypte(sectBuf, MSG_SECT_SIZE, 0);       /* encrypt      */
    if (fwrite(sectBuf, MSG_SECT_SIZE, 1, msgfl) != 1) {
	printf("zapMsgFil: write failed\n");
    }

    crypte(sectBuf, MSG_SECT_SIZE, 0);       /* decrypt      */
    sectBuf[0] = 0;
    crypte(sectBuf, MSG_SECT_SIZE, 0);       /* encrypt      */
    printf("\n%d sectors to be cleared\n", cfg.maxMSector);
    for (sect = 1l;  sect < cfg.maxMSector;  sect++) {
	printf("%u\r", sect);
	if (fwrite(sectBuf, MSG_SECT_SIZE, 1, msgfl) != 1) {
	    printf("zapMsgFil: write failed\n");
	}
    }
    crypte(sectBuf, MSG_SECT_SIZE, 0);       /* decrypt      */
    return TRUE;
}

SListBase KillInfoList = { NULL, NULL, NULL, NULL, NULL };
/*
 * indexRooms()
 *
 * This will build a RAM index to CTDLROOM.SYS, displays stats.
 */
void indexRooms()
{
    extern SListBase InfoMap;
    int  goodRoom, m, roomCount, slot;
    void CheckInfo(), InfoKill();

    ReadCitInfo();	/* setup room information */

    getRoom(LOBBY);
    strcpy(roomBuf.rbname, baseRoom);
    putRoom(LOBBY);

    zero_struct(roomBuf.rbflags);
    roomBuf.rbgen = 0;
    roomBuf.rbname[0] = 0;
    for (m = 0; m < MSGSPERRM; m++) {
	roomBuf.msg[m].rbmsgNo = 0l;
	roomBuf.msg[m].rbmsgLoc = 0;
    }

    strcpy(roomBuf.rbname, "Mail");
    roomBuf.rbflags.PUBLIC =
    roomBuf.rbflags.PERMROOM =
    roomBuf.rbflags.INUSE = TRUE;
    putRoom(MAILROOM);

    roomCount = 0;
    for (slot = 0;  slot < MAXROOMS;  slot++) {
	if (slot == 0)		     /* Ugly kludge */
	    strcpy(roomBuf.rbname, baseRoom);
	getRoom(slot);
	printf("Checking room #%d: ", slot);
	if (roomBuf.rbflags.INUSE == 1) {
	    /* roomBuf.rbflags.INUSE = 0; */
	    if (roomBuf.rbFlIndex >= (int) FloorSize)
		roomBuf.rbFlIndex = 0;

	    if (roomBuf.rbflags.PERMROOM != 1)   {

		for (m = 0, goodRoom = FALSE; m < MSGSPERRM && !goodRoom; m++) {
		    if (roomBuf.msg[m].rbmsgNo > cfg.oldest) {
			goodRoom    = TRUE;
		    }
		}
	    }
	    else goodRoom = TRUE;

	    roomBuf.rbname[NAMESIZE - 1] = 0;

	    if (goodRoom) {
		roomCount++;
		if (roomBuf.rbflags.SHARED) {
			FindHighestNative(&roomTab[slot].rtlastNetAll,
					&roomTab[slot].rtlastNetBB);
		}
		else {
			roomTab[slot].rtlastNetAll = 0l;
			roomTab[slot].rtlastNetBB = 0l;
		}
		noteRoom();
	    }
	    else {
		KillInfo(roomBuf.rbname);
		zero_struct(roomBuf.rbflags);
		noteRoom();
		putRoom(slot);
	    }
	}
        else {
	    noteRoom();
	}
	printf("%s\n",
	       (roomBuf.rbflags.INUSE == 1) ? roomBuf.rbname : "<not in use>");
    }
    printf(" %d of %d rooms in use\n", roomCount, MAXROOMS);

    RunList(&InfoMap, CheckInfo);
    RunList(&KillInfoList, (ITERATOR) KillInfo);
}

/*
 * CheckInfo()
 *
 * This function helps look for excess information.
 */
void CheckInfo(map)
NumToString *map;
{
    if (roomExists(map->string) == ERROR)
	AddData(&KillInfoList, strdup(map->string), NULL, FALSE);
}

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
 * FindHighestNative()
 *
 * This finds the highest native message in a room.
 */
void FindHighestNative(MSG_NUMBER *All, MSG_NUMBER *bb)
{
    int rover;
    theMessages *temp;

    temp = (theMessages *) GetDynamic(MSG_BULK);
    copy_ptr(roomBuf.msg, temp, MSGSPERRM);
    qsort(temp, MSGSPERRM, sizeof temp[0], msgSort);

    *All = 0l;
    *bb  = 0l;
    for (rover = 0; rover < MSGSPERRM; rover++) {
	if (temp[rover].rbmsgNo != 0l &&
    		temp[rover].rbmsgNo >= cfg.oldest &&
    		temp[rover].rbmsgNo <= cfg.newest &&
		     cfindMessage(temp[rover].rbmsgLoc, temp[rover].rbmsgNo)) {
	    if (strncmp(msgBuf.mbaddr, R_SH_MARK, strlen(R_SH_MARK)) == SAMESTRING ||
		strncmp(msgBuf.mbaddr, NON_LOC_NET, strlen(NON_LOC_NET)) == SAMESTRING)
		*All = temp[rover].rbmsgNo;
	    if (strncmp(msgBuf.mbaddr, LOC_NET, strlen(LOC_NET)) == SAMESTRING)
		*bb = temp[rover].rbmsgNo;
	    if (*bb != 0l && *All != 0l)
		break;
	}
    }
    free(temp);
}

/*
 * msgSort()
 *
 * This function sorts messages by their native msg id.
 */
int msgSort(theMessages *s1, theMessages *s2)
{
	if (s1->rbmsgNo < s2->rbmsgNo) return 1;
	if (s1->rbmsgNo > s2->rbmsgNo) return -1;
	return 0;
}

/*
 * noteRoom()
 *
 * This function will enter room into RAM index array.
 */
void noteRoom()
{
    int   i;
    MSG_NUMBER last;

    last = 0l;
    for (i = 0;  i < MSGSPERRM;  i++)  {
	if ((roomBuf.msg[i].rbmsgNo & S_MSG_MASK) > cfg.newest) {
	    roomBuf.msg[i].rbmsgNo = 0l;
	}
	if (roomBuf.msg[i].rbmsgNo > last) {
	    last = roomBuf.msg[i].rbmsgNo;
	}
    }
    roomTab[thisRoom].rtlastMessage = last	   ;
    strcpy(roomTab[thisRoom].rtname, roomBuf.rbname) ;
    roomTab[thisRoom].rtgen	    = roomBuf.rbgen  ;
    roomTab[thisRoom].rtFlIndex	= roomBuf.rbFlIndex;
    copy_struct(roomBuf.rbflags, roomTab[thisRoom].rtflags);
}

/*
 * zapRoomFile()
 *
 * This function erases and re-initializes CTDLROOM.SYS.
 */
char zapRoomFile()
{
    int i;

    if (!FirstInit) {
	printf("\nWipe room file? ");
	if (toUpper(simpleGetch()) != 'Y') return FALSE;
	printf("\n");
    }

    zero_struct(roomBuf.rbflags);

    roomBuf.rbgen	    = 0;
    roomBuf.rbname[0]	= 0;   /* unnecessary -- but I like it...  */
    for (i = 0;  i < MSGSPERRM;  i++) {
	roomBuf.msg[i].rbmsgNo =  0l;
	roomBuf.msg[i].rbmsgLoc = 0 ;
    }

    printf("maxrooms=%d\n", MAXROOMS);

    for (thisRoom = 0;  thisRoom < MAXROOMS;  thisRoom++) {
	printf("clearing room %d\r", thisRoom);
	putRoom(thisRoom);
	noteRoom();
    }
    printf("\n");

    /* Lobby> always exists -- guarantees us a place to stand! */
    thisRoom	    = 0	     ;
    strcpy(roomBuf.rbname, baseRoom)    ;
    roomBuf.rbflags.PERMROOM = TRUE;
    roomBuf.rbflags.PUBLIC   = TRUE;
    roomBuf.rbflags.INUSE    = TRUE;

    putRoom(LOBBY);
    noteRoom();

    /* Mail> is also permanent...       */
    thisRoom	    = MAILROOM      ;
    strcpy(roomBuf.rbname, "Mail")      ;
	/* Don't bother to copy flags, they remain the same (right?)    */
    putRoom(MAILROOM);
    noteRoom();

    /* Aide> also...		*/
    thisRoom	    = AIDEROOM      ;
    strcpy(roomBuf.rbname, "Aide")      ;
    roomBuf.rbflags.PERMROOM = TRUE;
    roomBuf.rbflags.PUBLIC   = FALSE;
    roomBuf.rbflags.INUSE    = TRUE;
    putRoom(AIDEROOM);
    noteRoom();

    return TRUE;
}

/*
 * logInit()
 *
 * This function indexes ctdllog.sys.
 */
void logInit()
{
    int i;
    int count = 0;
    SListBase Found = { NULL, NULL, NULL, NULL, NULL };
    extern SListBase MailForward;
    ForwardMail *forward;

    OpenForwarding();

#ifdef IS_RIGHT
    if (rewind(logfl) != 0) illegal("Rewinding logfl failed!");
#else
    rewind(logfl);
#endif
    /* clear logTab */
    for (i = 0; i < cfg.MAXLOGTAB; i++) logTab[i].ltnewest = 0l;

    /* load logTab: */
    for (thisLog = 0;  thisLog < cfg.MAXLOGTAB;  thisLog++) {
	printf("log#%3d", thisLog);
	getLog(&logBuf, thisLog);

	/* count valid entries:	     */
	if (logBuf.lbflags.L_INUSE == 1) {
	    count++;
	    printf("   %s", logBuf.lbname);
	    if ((forward = SearchList(&MailForward, logBuf.lbname)) != NULL) {
		KillData(&MailForward, logBuf.lbname);	/* harmless */
		AddData(&Found, forward, NULL, FALSE);
	    }
	}
	else printf("   <not in use>");
	printf("\n");

	/* copy relevant info into index:   */
	logTab[thisLog].ltnewest = logBuf.lblaston;
	logTab[thisLog].ltlogSlot= thisLog;
	if (logBuf.lbflags.L_INUSE == 1) {
	    logTab[thisLog].ltnmhash = hash(logBuf.lbname);
	    logTab[thisLog].ltpwhash = hash(logBuf.lbpw  );
	    logTab[thisLog].ltpermanent = logBuf.lbflags.PERMANENT;
	}
	else {
	    logTab[thisLog].ltnmhash = 0;
	    logTab[thisLog].ltpwhash = 0;
	}
    }
    printf(" logInit--%d valid log entries\n", count);
    printf("sortLog...\n");
    qsort(logTab, cfg.MAXLOGTAB, sizeof(*logTab), logSort);
    if (GetFirst(&MailForward) != NULL) {	/* implies irrelevant records */
	printf("Culling mail forwarding...\n");
	KillList(&MailForward);
	MoveAndClear(&Found, &MailForward);
	UpdateForwarding();
    }
}

/*
 * logSort()
 *
 * This function Sorts 2 entries in logTab.
 */
int logSort(LogTable *s1, LogTable *s2)
{
    if (s1->ltnmhash == 0 && s2->ltnmhash == 0)
	return 0;
    if (s1->ltnmhash == 0 && s2->ltnmhash != 0)
	return 1;
    if (s1->ltnmhash != 0 && s2->ltnmhash == 0)
	return -1;
    if (s1->ltnewest < s2->ltnewest)
	return 1;
    if (s1->ltnewest > s2->ltnewest)
	return -1;
    return 0;
}

/*
 * noteLog()
 *
 * This notes a logTab entry in RAM buffer in master index.
 */
void noteLog()
{
    int i, slot;

    /* figure out who it belongs between:	*/
    for (i = 0;  logTab[i].ltnewest > logBuf.lblaston;  i++);

    /* note location and open it up:	*/
    slot = i;
    slideLTab(slot, cfg.MAXLOGTAB-1);

    /* insert new record */
    logTab[slot].ltnewest       = logBuf.lblaston  ;
    logTab[slot].ltlogSlot      = thisLog	    ;
    logTab[slot].ltpwhash       = hash(logBuf.lbpw)  ;
    logTab[slot].ltnmhash       = hash(logBuf.lbname);
}

/*
 * slideLTab()
 *
 * This function slides bottom N slots in logTab down.  For sorting.
 */
void slideLTab(int slot, int last)
{
    int i;

    /* open slot up: (movmem isn't guaranteed on overlaps) */
    for (i = last - 1;  i >= slot;  i--)  {
	movmem(&logTab[i], &logTab[i + 1], sizeof (*logTab));
    }
}

/*
 * wrapup()
 *
 * This finishes up and writes ctdlTabl.sys out, finally.
 */
void wrapup(char onlyParams)
{
	printf("\ncreating ctdlTabl.sys table\n");

	if (!onlyParams) {
		if (!msgZap)  msgInit();
		if (!roomZap) indexRooms();
		if (!logZap)  logInit();
		netInit();
		if (mailCount)
			printf("%d of the messages were Mail\n", mailCount);
	}

	EventTab = (EVENT *) GetDynamic(cfg.EvNumber * sizeof *EventTab);

	DomainIntegrity();

	RoomInfoIntegrity();

	if (!FinalSystemCheck(onlyParams)) exit(2);

	if (!onlyParams) {
		if (cfg.BoolFlags.netParticipant)
			fclose(netfl);
		fclose(roomfl);
		fclose(msgfl);
		fclose(logfl);
	}

	RunList(&Events, EventWrite);
	printf("writeSysTab = %d\n", writeSysTab());
}

/*
 * zapLogFile()
 *
 * This erases & re-initializes ctdllog.sys.
 */
char zapLogFile()
{
    int  i;

    if (!FirstInit) {
	printf("\nWipe out log file? ");
	if (toUpper(simpleGetch()) != 'Y')   return FALSE;
	printf("\n");
    }

    /* clear RAM buffer out:			*/
    logBuf.lbflags.L_INUSE = FALSE;
    for (i = 0;  i < MAILSLOTS;  i++) {
	logBuf.lbMail[i].rbmsgLoc = 0l;
	logBuf.lbMail[i].rbmsgNo  = 0l;
    }
    for (i = 0;  i < NAMESIZE;  i++) {
	logBuf.lbname[i] = 0;
	logBuf.lbpw[i]   = 0;
    }

    /* write empty buffer all over file;	*/
    for (i = 0; i < cfg.MAXLOGTAB;  i++) {
	printf("Clearing log #%d\r", i);
	putLog(&logBuf, i);
	logTab[i].ltnewest = logBuf.lblaston;
	logTab[i].ltlogSlot= i;
	logTab[i].ltnmhash = hash(logBuf.lbname);
	logTab[i].ltpwhash = hash(logBuf.lbpw  );
    }
    return TRUE;
}

/*
 * netInit()
 *
 * This will Initialize RAM index for Ctdlnet.sys.
 */
void netInit()
{
    label temp;
    int i = 0;
    long length;

    if (!cfg.BoolFlags.netParticipant) return;
    totalBytes(&length, netfl);
    cfg.netSize = (int) (length / NB_TOTAL_SIZE);
    if (cfg.netSize)
	netTab = (NetTable *) GetDynamic(sizeof (*netTab) * cfg.netSize);
    else
	netTab = NULL;

    while (i < cfg.netSize) {
	getNet(i, &netBuf);
	normId(netBuf.netId, temp);
	netTab[i].ntnmhash = hash(netBuf.netName);
	netTab[i].ntidhash = hash(temp);
	strcpy(netTab[i].ntShort, netBuf.nbShort);
	copy_struct(netBuf.nbflags, netTab[i].ntflags);
	netTab[i].ntMemberNets = netBuf.MemberNets;
	printf("System %3d. %s\n", i,
	   (netBuf.nbflags.in_use) ? netBuf.netName : "<not in use>");
	i++;
    }

#ifdef WANT_THIS_CHECK
    if (cfg.MailHub != 0) {
	if (searchNameNet(cfg.MailHub + cfg.codeBuf, &netBuf) == -1)
	    illegal("Your #MailHub node was not found in your CtdlNet.Sys.");
	if (netBuf.nbRoute != -1 &&
		netTab[netBuf.nbRoute].ntGen == netBuf.nbRouteGen)
	    illegal("You must have a direct connect to your #MailHub.");
    }
#endif
}

/*
 * strCmpU()
 *
 * This is strcmp(), but ignoring case distinctions.  Found in most C libraries
 * as stricmp().  Someday we should switch over.
 */
int strCmpU(char s[], char t[])
{
    int  i;

    i = 0;

    while (toUpper(s[i]) == toUpper(t[i])) {
	if (s[i++] == '\0')  return 0;
    }
    return  toUpper(s[i]) - toUpper(t[i]);
}

/*
 * crashout()
 *
 * This handles a fatal error, for library functions.
 */
void crashout(char *str)
{
    illegal(str);
}

/*
 * cfindMessage()
 *
 * This function gets all set up to do something with a message.
 */
char cfindMessage(SECTOR_ID loc, MSG_NUMBER id)
{
    long atol();
    MSG_NUMBER here;
    extern struct mBuf mFile1;

    startAt(msgfl, &mFile1, loc, 0);

    do {
	getMessage(getMsgChar, FALSE, FALSE, TRUE);
	here = atol(msgBuf.mbId);
    } while (here != id &&  mFile1.thisSector == loc);

    return ((here == id));
}

/*
 * CheckFloors()
 *
 * This function will check the floors.
 */
void CheckFloors()
{
    SYS_FILE     tempName;
    FILE	 *flrfl;
    struct floor FloorBuf;
    extern char  *R_W_ANY, *WRITE_ANY;

    makeSysName(tempName, "ctdlflr.sys", &cfg.floorArea);
    if ((flrfl = fopen(tempName, R_W_ANY)) == NULL) {
	printf(" %s not found, creating new file.\n", tempName);
	if ((flrfl = fopen(tempName, WRITE_ANY)) == NULL)
	    illegal("?Can't create the floor file!");
    }
    strcpy(FloorBuf.FlName, cfg.codeBuf + cfg.MainFloor);
    FloorBuf.FlInuse = TRUE;
    fwrite(&FloorBuf, sizeof FloorBuf, 1, flrfl);
    totalBytes(&FloorSize, flrfl);
    FloorSize /= sizeof FloorBuf;
    fclose(flrfl);
}

/*
 * CheckNet()
 *
 * This creates the Ctdlnet.sys if needed.
 */
void CheckNet()
{
    SYS_FILE      tempName;

    makeSysName(tempName, "ctdlnet.sys", &cfg.netArea);
    if ((netfl = fopen(tempName, READ_ANY)) == NULL) {
	printf(" %s not found, creating new file.\n", tempName);
	if ((netfl = fopen(tempName, WRITE_ANY)) == NULL)
	    illegal("?Can't create the net file!");
    }
}

/*
 * NetParse()
 *
 * This function is responsible for parsing the network parameters.
 */
static char NetParse(char *line, int *offset, char *var, int arg)
{
    char temp[100];

    if (strcmp(var, "#NewNetPrivs")    == SAMESTRING) {
	cfg.BoolFlags.NetDft      = arg;
	return TRUE;
    } else if (strcmp(var, "#SCAN-NET-MESSAGES") == SAMESTRING) {
	cfg.BoolFlags.NetScanBad = TRUE;
	return TRUE;
    } else if (strcmp(var, "#SIEGENET") == SAMESTRING) {
	cfg.BoolFlags.SiegeNet = TRUE;
	return TRUE;
    } else if (strcmp(var, "#MAX-UNSTABLE") == SAMESTRING) {
	cfg.MaxNotStable = arg;
	return TRUE;
    } else if (strcmp(var, "#LD-DELAY"   )    == SAMESTRING) {
	cfg.LD_Delay = arg;
	return TRUE;
    } else if (strcmp(var, "#NETWORK"   )    == SAMESTRING) {
	cfg.BoolFlags.netParticipant = arg;
	return TRUE;
    } else if (strcmp(var, "#NET_AREA_SIZE") == SAMESTRING) {
	cfg.sizeArea = arg;
	return TRUE;
    } else if (strcmp(var, "#MAX_NET_FILE")    == SAMESTRING) {
	cfg.maxFileSize = arg;
	return TRUE;
    } else if (strcmp(var, "#callOutSuffix") == SAMESTRING) {
	readString(line, &cfg.codeBuf[*offset], TRUE);
	cfg.netSuffix = *offset;
	while (cfg.codeBuf[*offset])
	    (*offset)++;
	(*offset)++;
	return TRUE;
    } else if (strcmp(var, "#callOutPrefix") == SAMESTRING) {
	readString(line, &cfg.codeBuf[*offset], TRUE);
	DefaultPrefix = *offset;
	while (cfg.codeBuf[*offset])
	    (*offset)++;
	(*offset)++;
	return TRUE;
    } else if (strcmp(var, "#DialOut300") == SAMESTRING) {
	return ReadDialOut(line, ONLY_300, offset);
    } else if (strcmp(var, "#DialOut1200") == SAMESTRING) {
	return ReadDialOut(line, BOTH_300_1200, offset);
    } else if (strcmp(var, "#DialOut2400") == SAMESTRING) {
	return ReadDialOut(line, TH_3_12_24, offset);
    } else if (strcmp(var, "#DialOut4800") == SAMESTRING) {
	return ReadDialOut(line, B_4, offset);
    } else if (strcmp(var, "#DialOut9600") == SAMESTRING) {
	return ReadDialOut(line, B_5, offset);
    } else if (strcmp(var, "#DialOut14400") == SAMESTRING) {
	return ReadDialOut(line, B_6, offset);
    } else if (strcmp(var, "#DialOut19200") == SAMESTRING) {
	return ReadDialOut(line, B_7, offset);
    } else if (strcmp(var, "#DialOut38400") == SAMESTRING) {
	return ReadDialOut(line, B_8, offset);
    } else if (strcmp(var, "#DialOut56800") == SAMESTRING) {
	return ReadDialOut(line, B_9, offset);
    } else if (strcmp(var, "#nodeName" ) == SAMESTRING) {
	readString(line, &cfg.codeBuf[*offset], FALSE);
	NormStr(&cfg.codeBuf[*offset]);
	if (strLen(&cfg.codeBuf[*offset]) > 19)
	    illegal("nodeName too long; must be less than 20");
	if (strchr(&cfg.codeBuf[*offset], '_') != NULL ||
	    strchr(&cfg.codeBuf[*offset], '.') != NULL)
	    illegal("The characters '.' and '_' are illegal in node names!");
	cfg.nodeName    = *offset;
	while (cfg.codeBuf[*offset]) /* step over string     */
	    (*offset)++;
	(*offset)++;
	return TRUE;
    } else if (strcmp(var, "#nodeId"   ) == SAMESTRING) {
	readString(line, &cfg.codeBuf[*offset], FALSE);
	if (strLen(&cfg.codeBuf[*offset]) > 19)
	    illegal("nodeId too long; must be less than 20");
	cfg.nodeId      = *offset;
	while (cfg.codeBuf[*offset]) /* step over string     */
	    (*offset)++;
	(*offset)++;
	return TRUE;
    } else if (strcmp(var, "#MailHub"  ) == SAMESTRING) {
	readString(line, &cfg.codeBuf[*offset], FALSE);
	if (strLen(&cfg.codeBuf[*offset]) > 19)
	    illegal("MailHub too long; must be less than 20");
	cfg.MailHub      = *offset;
	while (cfg.codeBuf[*offset]) /* step over string     */
	    (*offset)++;
	(*offset)++;
	return TRUE;
    } else if (strcmp(var, "#DomainDisplay") == SAMESTRING) {
	readString(line, cfg.DomainDisplay, FALSE);
	if (strLen(cfg.DomainDisplay) >= sizeof cfg.DomainDisplay)
	    illegal("DomainDisplay is too long, must be less than 11");
	return TRUE;
    } else if (strcmp(var, "#nodeDomain") == SAMESTRING) {
	readString(line, &cfg.codeBuf[*offset], FALSE);
	if (strLen(&cfg.codeBuf[*offset]) > 19)
	    illegal("nodeDomain too long; must be less than 20");
	if (strchr(cfg.codeBuf + *offset, '_') != NULL ||
			strchr(cfg.codeBuf + *offset, '.') != NULL)
	    illegal("Domain names cannot have '_' or '.' in them.");
	cfg.nodeDomain = *offset;
	while (cfg.codeBuf[*offset]) /* step over string     */
	    (*offset)++;
	(*offset)++;
	return TRUE;
    } else if (strcmp(var, "#ServeDomain") == SAMESTRING) {
	readString(line, temp, FALSE);
	if (strLen(temp) > 19)
	    illegal("ServeDomain value is too long, must be less than 19 characters.");
	AddData(&Serves, strdup(temp), NULL, FALSE);
	cfg.DomainHandlers++;
	return TRUE;
    }

    return FALSE;
}

void TheAreaCheck()
{
    char bad = FALSE;

    if (!necessary[HELP]) {
	printf("The help stuff was not fully defined\n");
	bad = TRUE;
    }
    else AreaCheck(&cfg.homeArea);

    if (!necessary[LOG ]) {
	printf("The log stuff was not fully defined\n");
	bad = TRUE;
    }
    else AreaCheck(&cfg.logArea);

    if (!necessary[ROOM]) {
	printf("The room stuff was not fully defined\n");
	bad = TRUE;
    }
    else AreaCheck(&cfg.roomArea);

    if (!necessary[MSG ]) {
	printf("The msg stuff was not fully defined\n");
	bad = TRUE;
    }
    else AreaCheck(&cfg.msgArea);

    if (!necessary[FLOORA]) {
	printf("The floor stuff was not fully defined\n");
	bad = TRUE;
    }
    else AreaCheck(&cfg.floorArea);

    if (necessary[BIOA])
	AreaCheck(&cfg.bioArea);

    if (necessary[INFO_STUFF])
	AreaCheck(&cfg.infoArea);

    if (cfg.BoolFlags.mirror)
	AreaCheck(&cfg.msg2Area);

    if (!necessary[NET_STUFF] && cfg.BoolFlags.netParticipant) {
	printf("The net stuff was not fully defined\n");
	bad = TRUE;
    }
    else if (!necessary[DOM_STUFF] && cfg.BoolFlags.netParticipant) {
	printf("The domain stuff was not fully defined\n");
	bad = TRUE;
    }
    else if (cfg.BoolFlags.netParticipant) {
	AreaCheck(&cfg.netArea);
	AreaCheck(&cfg.domainArea);
    }

    if (!necessary[CALL] && cfg.Audit) {
	printf("The call stuff was not fully defined\n");
	bad = TRUE;
    }
    else if (cfg.Audit)
	AreaCheck(&cfg.auditArea);

    if (necessary[HOLD])
	AreaCheck(&cfg.holdArea);

    if (bad)
	illegal("See above.");
}

void CheckBaseroom()
{
	SYS_FILE      tempName;

	if (stricmp(baseRoom, OldBaseRoom) != 0) {
		makeSysName(tempName, "ctdlroom.sys", &cfg.roomArea);
		/* open room file */
		if (roomfl != NULL ||
				(roomfl = fopen(tempName, R_W_ANY)) != NULL) {
			getRoom(LOBBY);
			strcpy(roomBuf.rbname, baseRoom);
			putRoom(LOBBY);
			noteRoom();
		}
	}
}
