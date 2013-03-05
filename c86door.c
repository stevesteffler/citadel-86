/*
 *			     C86Door.c
 *
 *	External door shell functions, derived from Gary Meadow's
 *	ASGDOOR.C program.  Note this is not a portable file.
 */

#include "c86door.h"
#include "dos.h"
#include "time.h"

/*
 *				History
 *
 * 97Jul21  HAW Return to start dir after termination of door.
 * 96Aug18  HAW Support for preemptive events not being interfered with.
 * 91Aug19  HAW Support for CHAIN.TXT. (1.13)
 * 89Dec01  HAW Support for PCBOARD.SYS. (1.8)
 * 89Oct25  HAW Support for DOOR.SYS of DoorWay. (1.5)
 * 88Dec16  HAW Modified for my own style of doing things.
 * 88Dec03  HAW Received from Gary Meadows!
 */

int locDisk;
char locDir[60];
char *Months[] = { "",
	"Jan", "Feb", "Mar",
	"Apr", "May", "Jun",
	"Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec"
};

void main(void);
void ParamBuild(Transition *info, char *target, char *p);
void MSDOSparse(char *theDir, char *drive);
long CurAbsolute(void);
void GetDate(int *year, int *month, int *day, int *hours, int *minutes);
void CallMsg(char *fn, char *str);

/*
 * main()
 *
 * Main manager.
 */
void main()
{
	DoorData   drBuf;
	Transition drInf;
	FILE	   *btfd, *drfd, *qbbs;
	char	   line[128], drive, dir[50];
	int	   result, byr, bmon, bdy, bhr, bmin, ayr, amon, ady, ahr, amin;
	long	   babs, aabs;
	char	   *x, fName[20];
	time_t	   now; 
	char	   *timest; 
	long	   max_time;

	printf("\nCitadel-86 external door controller Version 1.16\n");
	printf("(Derived from the Asgard-86 door controller V1.1)\n%s\n\n",
							     COPYRIGHT);
	locDisk = getdisk();
	getcwd(locDir,60);

	if ((btfd=fopen("dorinfo2.def","rb"))==NULL) {
		printf("Door failure - file dorinfo2.def missing\n ");
		exit(1);
	}

	fread(&drInf, sizeof(drInf), 1, btfd);
	fclose(btfd);

	/* drInf.DoorDir is the full name of CitaDoor.sys -- doors data */
	if ((drfd = fopen(drInf.DoorDir, "rb")) == NULL) {
		printf("No doors available!\n");
		exit(1);
	}

	if (fseek(drfd, sizeof drBuf * drInf.DoorNumber, 0) != 0) {
		printf("Couldn't seek to %d in the doors file!\n",
			drInf.DoorNumber);
		exit(1);
	}

	if (fread(&drBuf, sizeof(drBuf), 1, drfd) <= 0) {
		printf("Couldn't read from the doors file!\n");
		exit(1);
	}
	fclose(drfd);

	/* Now get us to the right drive/directory. */
	strcpy(dir, drBuf.location);

	MSDOSparse(dir, &drive);
	setdisk(drive-'A');

	if (chdir(dir)) {
		printf("DOORS ERROR! Can not find directory %s!\n", drBuf.location);
	}
	else {      /* In right place, so now execute the damn door */

		max_time = min(60, (drInf.TimeToNextEvent / 60));

		if ((qbbs=fopen("dorinfo1.def", "wt")) != NULL) {
			fprintf(qbbs, "%s\n%s\n\nCOM%d\n%d0 BAUD,N,8,1\n0\n",
			  	drInf.System, drInf.Sysop,
			  	(drInf.bps == 0) ? 0 : drInf.Port,
			  	drInf.bps);
			fprintf(qbbs, "%s\n\n\n%d\n0\n%ld\n0\n",
			  	drInf.UserName, drInf.AnsiType, max_time);
			fclose(qbbs);
		}

		/* we also support door.sys. */
		if ((qbbs=fopen("door.sys", "wt")) != NULL) {
			fprintf(qbbs, "%s\n%d\n%d0\n30000\n%c\nS\n",
				drInf.UserName,
			   	(drInf.bps == 0) ? 0 : drInf.Port, drInf.bps,
			   	drInf.AnsiType ? 'G' : 'M');
			fclose(qbbs);
		}

		/* Wildcat! support.  Some of these fields are ridiculous. */
		if ((qbbs=fopen("callinfo.bbs", "wt")) != NULL) {
			fprintf(qbbs, "%s\n", drInf.UserName);
			switch (drInf.bps) {
				case 0: result = 5; break;
				case 30: result = 1; break;
				case 120: result = 2; break;
				case 240: result = 0; break;
				case 960: result = 3; break;
				default: result = 5;
			}
			fprintf(qbbs, "%d\n\n\n%ld\n%s\n\n", result, max_time,
					drInf.AnsiType ? "COLOR" : "MONO");
			fprintf(qbbs, "%d\n\n\n\n\n\n\n\n\n\n\nEXPERT\n",
					drInf.UserLog);
			fprintf(qbbs, "Ascii\n\n1\n23\n0\n0\n0\n8  { Databits }\n");
			fprintf(qbbs, "%s\n", (drInf.bps == 0) ? "LOCAL" : "REMOTE");
			fprintf(qbbs, "COM%d\n\n%d0\nTRUE\n", drInf.Port, drInf.bps);
			fprintf(qbbs, "Normal Connection\n\n0\n%d\n", drInf.DoorNumber);
			fclose(qbbs);
		}

		/* PCboard support ... sort of */
		if ((qbbs=fopen("pcboard.sys", "wt")) != NULL) {
			if (strchr(drInf.UserName, ' ') != NULL)
				strcpy(fName, strchr(drInf.UserName, ' '));
			else
				strncpy(fName, drInf.UserName, 15);
 			now= 0; 
 			now= time(&now); 
 			timest= ctime(&now); 

 			x= &(timest[11]); 
 			x[5]= NULL; 
			fprintf(qbbs,
 				"-1-1%s-1   %d%s%s%sXXXXXXXXXXXX999999%d0000%s01-1-1-1-"\
 				"1-1-1-1-1-19900000000 0     00", 
				"0", drInf.bps * 10, drInf.UserName, fName, 
				(drInf.AnsiType) ? "-1" : "0",
				20 * 60, x);

			fclose(qbbs);
		}

		if ((qbbs=fopen("chain.txt", "wt")) != NULL) {
			fprintf(qbbs, "%d\n%s\n%s\n",
				drInf.UserLog, drInf.UserName,
				drInf.UserName);
			fprintf(qbbs, "\n\n\n\n");
			fprintf(qbbs, "\n");	/* date goes here */
			fprintf(qbbs, "%d\n25\n0\n0\n0\n", drInf.UserWidth);
			fprintf(qbbs, "%d\n", drInf.AnsiType);
			fprintf(qbbs, "%d\n0\n\n\n", (drInf.bps == 0) ? 0 : 1);
			fprintf(qbbs, "%scalllog.sys\n", drInf.AuditLoc);
			if (drInf.bps == 0)
				fprintf(qbbs, "KB\n");
			else
				fprintf(qbbs, "%d\n", drInf.bps * 100);
			fprintf(qbbs, "%d\n", drInf.Port);
			fprintf(qbbs, "%s\n", drInf.System);
			fprintf(qbbs, "%s\n0\n0\n0\n0\n0\n0\n8N1\n", drInf.Sysop);
			fclose(qbbs);
		}

		ParamBuild(&drInf, line, drBuf.CommandLine);

		GetDate(&byr, &bmon, &bdy, &bhr, &bmin);
		babs = CurAbsolute();
		result = system(line);
		if (result)
			printf("\n Error executing program, ErrLevel = %d\n ", result);
		aabs = CurAbsolute();
		GetDate(&ayr, &amon, &ady, &ahr, &amin);
		sprintf(line,
		"    %s used %s %2d%s%02d %d:%02d - %d:%02d (%ld:%02ld)\n",
		drInf.UserName, drBuf.entrycode, byr, Months[bmon], bdy,
		bhr, bmin, ahr, amin, ((aabs - babs) / 60l),
						((aabs - babs) % 60l));

		/*
		 * 1.16 -- return to the starting area
		 */
		setdisk(drive-'A');

		if (chdir(dir) == 0) {
			unlink("dorinfo1.def");
			unlink("door.sys");
			unlink("callinfo.bbs");
			unlink("pcboard.sys");
			unlink("chain.txt");
		}
	}

	/*
	 * Now, return to our home directory (should be where C-86 was executing).
	 */
	setdisk(locDisk);
	chdir(locDir);
	drInf.Seconds = aabs - babs;

	if ((btfd=fopen("dorinfo2.def","wb"))==NULL) {
		printf("Couldn't open door dorinfo2.def for return volley\n ");
	}
	else {
		fwrite(&drInf, sizeof(drInf), 1, btfd);
		fclose(btfd);
	}

	if (strlen(drInf.AuditLoc) != 0) {
		sprintf(dir, "%s%s", drInf.AuditLoc, "dooruse.sys");
		if ((qbbs = fopen(dir, "a")) == NULL) {
			printf("Couldn't open %s (%d) (%d)!", dir, strlen(dir), errno);
		}
		else {
			fprintf(qbbs, line);
			fclose(qbbs);
		}
	}
}

/*
 * ParamBuild()
 *
 * Like Asgard, build a parameter line to C86 spec.
 */
void ParamBuild(Transition *info, char *target, char *p)
{
	while (*p) {
		switch (*p) {
		case DV_BAUD:
			sprintf(target, "%d", info->bps * 10);
			while (*target) target++;
			break;
		case DV_BPS:
			sprintf(target, "%d", info->bps);
			while (*target) target++;
			break;
		case DV_DCE:
			sprintf(target, "%d", info->DCE);
			while (*target) target++;
			break;
		case DV_PORT:
			if (info->bps == 0)
				strcpy(target, "LOCAL");
			else
				sprintf(target, "%d", info->Port);
			while (*target) target++;
			break;
		case DV_USER_NAME:
			sprintf(target, "\"%s\"", info->UserName);
			while (*target) target++;
			break;
		case DV_USER_NUM:
			sprintf(target, "%d", info->UserLog);
			while (*target) target++;
			break;
		case DV_ANSI:
			sprintf(target, "%d", info->AnsiType);
			while (*target) target++;
			break;
		case DV_WIDTH:
			sprintf(target, "%d", info->UserWidth);
			while (*target) target++;
			break;
		case DV_PORT_2:
			if (info->bps == 0)
				strcpy(target, "LOCAL");
			else
				sprintf(target, "COM%d", info->Port);
			while (*target) target++;
			break;
		case DV_MNP:
			if (info->mnp) {
				strcpy(target, "MNP");
				while (*target) target++;
			}
			break;
		default:
			*target = *p;
			target++;
			break;
		}
		p++;
	}
	*target = 0;
}

/*
 * MSDOSparse()
 *
 * This parses a string.
 */
void MSDOSparse(char *theDir, char *drive)
{
	if (theDir[1] == ':') {
		*drive = toupper(theDir[0]);
		strcpy(theDir, theDir+2);
	}
	else {
		*drive = locDisk;
	}
}

/** Following functions stolen from C-86 source files **/

/*
 * CurAbsolute()
 *
 * This gets the current time in absolute seconds.
 */
long CurAbsolute()
{
	struct date dateblk;
	struct time timeblk;

	getdate(&dateblk);
	gettime(&timeblk);

	return dostounix(&dateblk, &timeblk);
}

/*
 * getRawDate()
 *
 * This gets the raw date from MSDOS.
 */
void GetDate(int *year, int *month, int *day, int *hours, int *minutes)
{
	struct date dateblk;
	struct time timeblk;

	getdate(&dateblk);
	gettime(&timeblk);

	*year  = dateblk.da_year;
	*month = dateblk.da_mon;
	*day  = dateblk.da_day ;
	*hours = timeblk.ti_hour;
	*minutes = timeblk.ti_min ;
}
