/*
 *				Arch.c
 * Archival functions.
 */

#include "ctdl.h"

/*
 *	                        history
 *
 * 89Jan09 HAW  Completed initial version.
 */

/*
 *	                        Contents
 *
 *	AskForNSMap()		search function
 *	ChkNtoStr()             testing function for archival stuff
 *	ChkStrtoN()             checks strings against each other
 *	EatNMapStr()            eat an archival string.
 *	FreeNtoStr()            frees a data node of archive type
 *	NtoStrInit()            utility initialization function
 *	WrtNtoStr()             writes to ctdlarch.sys
 */

/*
 * Arch_base
 *
 * This list contains the list of archive files for archive rooms.  The
 * information consists of the room slot number, the string representing
 * the filename to place messages in, and the max size of the file.
 */
SListBase  Arch_base = { NULL, ChkNtoStr, NULL, FreeNtoStr, EatArchRec };

extern FILE *upfd;
/*
 * EatNMapStr()
 *
 * This will eat a string formatted as "<digit> <string>".  It is used as
 * part of a list to read information in from text files that is in a
 * generic format and place it in a multipurpose data structure.
 */
void *EatNMapStr(char *line)
{
    char    *s;

    if ((s = strchr(line, ' ')) == NULL)
        return NULL;

    return NtoStrInit(atoi(line), s+1, 0, FALSE);
}

/*
 * EatArchRec()
 *
 * This function creates an archive record from the line passed in.
 */
void *EatArchRec(char *line)
{
    char *s, *c;

    if ((s = strchr(line, ' ')) == NULL)
	return NULL;

    if ((c = strchr(s + 1, ' ')) == NULL)
	return NtoStrInit(atoi(line), s+1, 0, FALSE);

    *c++ = 0;
    return NtoStrInit(atoi(line), s+1, atoi(c), FALSE);
}

/*
 * NtoStrInit()
 *
 * This is a utility function to generate temporary or permanent data nodes
 * for searching or adding to Archive-type lists.
 */
void *NtoStrInit(int num, char *str, int num2, char needstatic)
{
    NumToString *temp;
    static NumToString t = { 0, NULL };

    if (needstatic) {
        temp = &t;
        if (temp->string != NULL) free(temp->string);
    }
    else
        temp = (NumToString *) GetDynamic(sizeof *temp);

    temp->num  = num;
    temp->num2 = num2;
    temp->string = strdup(str);
    return (void *) temp;
}

static char ReturnNum2 = FALSE;
/*
 * GetArchSize()
 *
 * This function gets the requested size of the archive.  This is acquired
 * from the list Arch_base.  If the archive can't be found (never happens)
 * then 0 is returned.
 */
int GetArchSize(int num)
{
    UNS_16 *x;

    ReturnNum2 = TRUE;
    x = SearchList(&Arch_base, NtoStrInit(num, "", 0, TRUE));
    ReturnNum2 = FALSE;
    if (x == NULL) return 0;
    return (int) *x;
}

/*
 * ChkNtoStr()
 *
 * This function is used to search lists which use the NumToString lists.  It
 * searches using the numerical value as a key.
 */
void *ChkNtoStr(NumToString *d1, NumToString *d2)
{
	if (d1->num == d2->num)
		return (ReturnNum2) ? (void *) &d1->num2 : (void *) d1->string;
	return NULL;
}

/*
 * ChkStrtoN()
 *
 * This function is used to search lists which use the NumToString lists.  It
 * searches using the string value as a key.
 */
void *ChkStrtoN(NumToString *d1, char *string)
{
    if (strCmpU(d1->string, string) == 0)
        return &d1->num;
    else return NULL;
}

/*
 * ChkStrForElement()
 *
 * This function is used to search lists which use the NumToString lists.  It
 * searches using the string value as a key, but returns the entire element.
 */
void *ChkStrForElement(NumToString *d1, char *string)
{
    if (strCmpU(d1->string, string) == 0)
        return d1;
    else return NULL;
}

/*
 * WrtNtoStr()
 *
 * This function is used to generically update files with the information
 * contained in NumToString structures.  NB: FILE variable upfd must be opened
 * by the user.
 */
void WrtNtoStr(NumToString *d)
{
    fprintf(upfd, "%d %s\n", d->num, d->string);
}

/*
 * WrtArchRec()
 *
 * This function writes an archive record out.  NB: FILE variable upfd must be
 * opened by the user.
 */
void WrtArchRec(NumToString *d)
{
    fprintf(upfd, "%d %s %d\n", d->num, d->string, d->num2);
}

/*
 * FreeNtoStr()
 *
 * This function frees a general use NumToString node.  We use this rather
 * than free because the field string is also dynamically allocated.
 */
void FreeNtoStr(NumToString *d)
{
    free(d->string);
    free(d);
}

/*
 * AskForNSMap()
 *
 * This function satisfies a request for a search of a list for the map value
 * of a given integer.  If fails, returns pointer to a zero length string.
 */
char *AskForNSMap(SListBase *base, int val)
{
    char *check;
    static char *zero = "";

    if ((check = (char *) SearchList(base, NtoStrInit(val, "", 0, TRUE))) != NULL)
        return check;

    return zero;
}
