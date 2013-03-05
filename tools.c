/*
 *				tools.c
 *
 * Random functions for both Ctdl and it's utilities.
 */

/*
 *				history
 *
 * 90Aug26 HAW  Created.
 */

#include "ctdl.h"

/*
 *				Contents
 *
 *	NormStr()		Deletes leading trailing blanks etc.
 *	PrintPretty()		Print numbers prettily.
 *	normId()		Normalizes a node id.
 *	hash()			Hashes a string to an integer.
 *      ReadDate()		Reads a date from the user
 *	CleanEnd()		Clears trailing blanks off.
 *	lbyte()			Finds the 0 byte of a string.
 */

/*
 * NormStr()
 *
 * This function Deletes leading trailing blanks etc.
 */
void NormStr(char *s)
{
    char *pc;

    pc = s;

    /* find end of string   */
    while (*pc)   {
	if (*pc < ' ')   *pc = ' ';   /* zap tabs etc... */
	pc++;
    }

    /* no trailing spaces: */
    while (pc>s  &&  isSpace(*(pc-1))) pc--;
    *pc = '\0';

    /* no leading spaces: */
    while (*s == ' ') {
	for (pc=s;  *pc;  pc++)    *pc = *(pc+1);
    }

    /* no double blanks */
    for (;  *s;)   {
	if (*s == ' '   &&   *(s+1) == ' ')   {
	    for (pc=s;  *pc;  pc++)    *pc = *(pc+1);
	}
	else s++;
    }
}

static long StartDiv = 1000000l;
static char FirstFlag = TRUE;
/*
 * PrintPretty()
 *
 * This will pretty print a long with commas.
 */
char *PrintPretty(long s, char *result)
{
	if (StartDiv == 1) {
		sprintf(result, FirstFlag ? "%ld" : "%03ld", s);
		FirstFlag = TRUE;
		StartDiv  = 1000000l;
		return result;
	}
	if (s >= StartDiv) {
		sprintf(result, FirstFlag ? "%ld," : "%03ld,", s / StartDiv);
		FirstFlag = FALSE;
		s %= StartDiv;
		StartDiv /= 1000;
		PrintPretty(s, result + strlen(result));
	}
	else {
		StartDiv /= 1000;
		PrintPretty(s, result);
	}
	return result;
}

/*
 * normId()
 *
 * This function normalizes a node id.
 */
char normId(label source, label dest)
{
    int digitcount = 0;

    while (!isalpha(*source) && *source)
	source++;
    if (!*source) return FALSE;
    *dest++ = toUpper(*source++);
    while (!isalpha(*source) && *source)
	source++;
    if (!*source) return FALSE;
    *dest++ = toUpper(*source++);
    while (*source) {
	if (isdigit(*source)) {
	    *dest++ = *source;
	    digitcount++;
	}
	source++;
    }
    *dest = '\0';
    return (digitcount > 8);
}

/*
 * hash()
 *
 * This function hashes a string to an integer.
 */
UNS_16 hash(char *str)
{
    UNS_16  h, shift;

    for (h=shift=0;  *str;  shift=(shift+1)&7, str++) {
	h ^= (toUpper(*str)) << shift;
    }
    return h;
}

/*
 * CleanEnd()
 *
 * This function cleans up a message trailer for later display via Continue or
 * .EH.  Inspired by Glen Heinz (MacCitadel).
 */
char *CleanEnd(char *text)
{
    char *ptr;
    int  wc, lc;	/* Word Count and Letter Count */

    if (strLen(text) == 0) return text;
    ptr = lbyte(text) - 1;      /* End of text of msg */
	/*
	 * Strip trailing whitespace.  We structure the loop this
	 * way to avoid any chance of accidentally accessing memory outside
	 * of the memory area.
	 */
    while (ptr != text - 1) {
	if (!(*ptr == ' ' || *ptr == NEWLINE || *ptr == TAB)) break;
	ptr--;
    }
    ptr++;	/* point at byte following last significant character */

    *ptr = 0;	/* tie it off with a NULL */
		/* Now we want to find a "preferred place" */
    for (wc = lc = 0, ptr--; wc < 4 && ptr > text && lc < 35; ptr--, lc++) {
	if (*ptr == ' ') wc++;
	if (*ptr == NEWLINE) break;     /* can't go beyond embedded NEWLINE */
    }

    if (ptr == text) return ptr;	/* if msg is empty or < 35 chars long */
    /* else */       return (ptr + 1);	/* else return "favored" spot */
}

/*
 * lbyte()
 *
 * This function finds the 0 byte of a string, returns pointer to it...
 */
char *lbyte(char *l)
{
    while (*l) l++;
    return l;
}

char CallCrash;
/*
 * CallMsg()
 *
 * This function is a generic message writing mechanism.  Given a filename, it
 * appends the given message to the end of said file.
 */
void CallMsg(char *fn, char *str)
{
    FILE	*fd;
    extern char *A_C_TEXT;

    if ((fd = fopen(fn, A_C_TEXT)) != NULL) {
	fprintf(fd, "%s\n", str);
	fclose(fd);
    }
    else {
	CallCrash = TRUE;
	printf("Failure: filename is %s, errno is %d", fn, errno);
	crashout("CallMsg error!");
    }
}

/*
 * ReadDate()
 *
 * This function interprets the string and returns it in seconds.
 */
int ReadDate(char *date, long *RetTime)
{
    static char *MonthTab[] = {
	"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
	"JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
    };
    int rover, found;
    int   month, day, hours, minutes, seconds, milli;
    label mon;
    struct date dptr;
    struct time tptr;
    char *d = date;

    if (strLen(date) == 0) return FALSE;

    if (isdigit(date[0])) {
	dptr.da_year = atoi(date) + 2000;
	while (isdigit(*date)) date++;
    }
    else {
	getRawDate(&dptr.da_year, &month, &day, &hours, &minutes, 
							&seconds, &milli);
    }
    for (rover = 0; isalpha(*date); date++, rover++)
	mon[rover] = toUpper(*date);

    mon[rover] = 0;

    if (rover == 0) {
	if (isdigit(d[0])) {
	    *RetTime = CurAbsolute() - (atol(d) * 86400l);
	    return TRUE;
	}
	return ERROR;
    }

    for (found = rover = 0; rover < NumElems(MonthTab); rover++)
	if (strncmp(mon, MonthTab[rover], strLen(mon)) == SAMESTRING) {
	    found++;
	    dptr.da_mon = rover + 1;
	}

    if (found != 1) return ERROR;

    if ((dptr.da_day = atoi(date)) == 0) return ERROR;
    zero_struct(tptr);
    *RetTime = dostounix(&dptr, &tptr);
    return TRUE;
}

/*
 * CurAbsolute()
 *
 * This gives the current time in absolute terms.
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
 * WriteAList()
 *
 * This writes a Num->String list to disk.
 */
void WriteAList(SListBase *base, char *fn, void (*func)())
{
    SYS_FILE      name;
    extern FILE   *upfd;
    extern CONFIG cfg;

    makeSysName(name, fn, &cfg.roomArea);
    if ((upfd = fopen(name, WRITE_TEXT)) != NULL) {
	RunList(base, func);
	fclose(upfd);
    }
    else printf("WARNING: unable to open %s!!\n", name);
}

