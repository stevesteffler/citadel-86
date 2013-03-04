/*
 *				2ndfmt.h
 *
 * Header file for common definitions concerning secondary node lists.
 */

#ifndef SECONDARY_HEADER

#define SECONDARY_HEADER

#include <stdio.h>
#include "sysdep.h"

void NormStr(char *s);

#define BUCKETCOUNT	36
#define VERS_SIZE	20
#define TABLE_VERS	1

#define DUP		0x01

#define FMT_COPYRIGHT "Copyright (c) 1990-1993 by Hue, Jr."

typedef struct {
	UNS_32 offset;		/* absolute jump point in file */
} JumpInfo;

#endif
