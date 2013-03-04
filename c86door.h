/*
 *				C86Door.h
 *
 * Header file for doors. (System dependent file.)
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "dir.h"
#include "ctype.h"

/*
 *			      History
 *
 * 88Dec16  HAW Created.
 */

#ifndef CTDL_HEADER

typedef char label[20];

#define TRUE		1
#define FALSE		0
#define SAMESTRING      0

#define COPYRIGHT       "Copyright (c) 1988 - 1996 by Hue, Jr."

#endif

/* Name of data file */
#define DOOR_DATA       "citadoor.sys"

/* These define privileges. */
#define DOOR_ANYONE	0
#define DOOR_AIDE	0x01
#define DOOR_SYSOP	0x02
#define DOOR_CON	0x04
#define DOOR_MODEM	0x08
#define DOOR_AUTO	0x10
#define DOOR_NEWUSER	0x20
#define DOOR_ONLOGIN	0x40

/*
 * These constants are used for parameter variable identification.  We're going
 * to do this in a radically different manner than Gary did it.  When the
 * string is processed, each character is checked.  Any matching the following
 * constants are variables and will be substituted for in C86Door.
 */
#define DV_BAUD		1
#define DV_BPS		2
#define DV_PORT		3
#define DV_USER_NAME    4
#define DV_USER_NUM     5
#define DV_ANSI		6
#define DV_PORT_2	8
#define DV_WIDTH	11
#define DV_MNP		13
#define DV_DCE		12

typedef struct {
    char  entrycode[6];		/* What user uses to specify a door     */
    char  location[50];		/* door location			*/
    char  description[80];	/* description of door			*/
    char  flags;		/* sysop, aides, or everyone		*/
    char  CommandLine[100];	/* list of parameters.			*/
    int   TimeLimit;		/* time limit for this door		*/
    label RoomName;		/* room name				*/
} DoorData;

typedef struct {
    int   DoorNumber;		/* Which door was selected?		*/
    int   UserLog;		/* Which user is it (for return)	*/
    int   RoomNum;		/* Which room were we in?		*/
    long  DLtime;		/* How much time spent downloading?     */
    char  Port;			/* What COM port?		*/
    int   bps;			/* DTE bps */
    int   DCE;			/* Data connect */
    char  AnsiType;
    label UserName;		/* name of user		*/
    label Sysop;		/* Sysop -- QBBS info		*/
    label System;		/* System name -- QBBS info	     */
    char  DoorDir[50];		/* Where the doors info is stored.      */
    char  AuditLoc[100];	/* So we can do record keeping.	 */
    long  Seconds;		/* Fill this in before return!	  */
    int   UserWidth;		/* Width of user's screen		*/
    char  mnp;			/* MNP connection?	*/
    long  TimeToNextEvent;	/* time 'til next demand event */
} Transition;


#ifdef CTDL_HEADER

char doDoor(char x);
char BackFromDoor(void);
char NoTimeForDoor(int which, DoorData *DoorInfo);

#endif
