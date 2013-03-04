/*
 * SList.C
 *
 * Copyright (c) 1989 by Hue, Jr.  All Rights Reserved.
 *
 * This is a generic, messy list handler.
 * VARIANT: this one automatically sorts the list where allowed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "slist.h"

#define TRUE    1
#define FALSE   0

/*
 * History
 *
 * 89Jan22 HAW  Variant for keeping sorted lists.
 * 89Jan01 HAW  Completed initial version.
 */

/************************************************************************/
/*			      Contents				*/
/*								      */
/*		      Generic Functions			       */
/*								      */
/*      AddData()	       Add an element of data to a list	*/
/*      KillData()	      remove an element of data from a list   */
/*      KillList()	      Destroys a list			 */
/*      MakeList()	      Construct a disk-based list in memory   */
/*      RunList()	       run a function on a list		*/
/*      SearchList()	    search a list for a given data element  */
/*								      */
/************************************************************************/

/*
 * Usage
 *
 * This module contains a generic list handler.
 *
 *  o Initialization - before a list can contain things, it must be
 *  initialized.  Initialization consists of declaring some variable to
 *  be of type SListBase (this variable will be what you use to access
 *  the list), and ensuring it is initialized correctly before use.
 *  Initialization may be accomplished either via initializers or
 *  manually.  In any case, this is what each field must be initialized
 *  to:
 *  - start -- initialize to NULL in ALL cases.
 *  - CheckIt -- initialize to point to a function capable of comparing
 *	two data structs for equality on some arbitrary, meaningful
 *	quality (specifically NOT the value of the void pointer itself!).
 *	If equal, it should return a pointer to some value which will be
 *	useful in that context, otherwise NULL.
 *  - cmp -- initialize to point to a function which accepts to two void *
 *	variables, i.e., must like qsort: cmp(void *s1, void *s2).  This
 *	function should return < 0 if s1 < s2, > 0 if s1 > s2, == 0 if the
 *	two variables are 'equal'.  If this field is NULL, then the list 
 *	will not be sorted.
 *  - FreeFunc -- some function capable of freeing an unneeded data
 *	instance from the list.
 *  - EatLine -- some function capable of digesting a line of data from
 *	a text file and producing a pointer to a new data instance with
 *	the results of the digestion entombed within for later use.
 *
 *  o Briefly, here are the currently supported services for list
 *  handling.
 *  - MakeList will construct a list from the given text file, using the
 *	EatLine function outlined above.
 *  - AddData will add a single data element to the list.  Data element
 *	in toto must be provided by the user.  Optionally will kill
 *	duplicates (detected using CheckIt function) from list, optionally
 *	will "write" list after addition (very blind - check code).
 *  - KillData will kill the specified data element from the list, using
 *	CheckIt to find them.  All instances will be deleted.
 *  - KillList will kill the list.  (Not implemented yet.)
 *  - SearchList will search the list for the given data element, using
 *	CheckIt to detect it.  If found, a void * will be returned to the
 *	caller, for its own interpretation.  The pointer returned will be
 *	the same as that returned by the CheckIt function.
 *  - RunList will apply the specified function to all members of the
 *	list.
 *
 *  This module needs the function void *GetDynamic(int size) to be
 *  defined as a function which returns size bytes of dynamic memory.
 */

#ifdef OLD_STYLE
void *GetDynamic(int size);
#else
void *(*slistmalloc)(unsigned size) = malloc;
void  (*slistfree)(void *)     = free;
#endif

/*
 * MakeList(SListBase *base, char *fn, FILE *fd)
 *
 * This function will construct a list by reading from a text file and asking 
 * a specified function to process the text.  If strlen(fn) == 0, it will use 
 * File fd passed in, otherwise it will try to open fn in text mode.
 */
char MakeList(base, fn, fd)
SListBase *base;
char	 *fn;
FILE	 *fd;
{
	char	line[160];
	void	*temp;

	if (strlen(fn)) {
		if ((fd = fopen(fn, "r")) == NULL)
			return FALSE;
	}

	while (GetAString(line, sizeof line, fd) != NULL) {
		if ((temp = (*base->EatLine)(line)) != NULL) {
			AddData(base, temp, NULL, FALSE);
		}
	}
	if (strlen(fn)) fclose(fd);

	return TRUE;
}

/*
 * char *GetAString(char *line, int size, FILE *fd)
 *
 * Gets a string from the file, chops off the EOL.  If EOF, returns NULL.
 */
char *GetAString(char *line, int size, FILE *fd)
{
	char *s;

	if (fgets(line, size, fd) != NULL) {
		if ((s = strchr(line, '\n')) != NULL)
			*s = 0;
		if ((s = strchr(line, '\r')) != NULL)
			*s = 0;
		return line;
	}

	return NULL;
}

/************************************************************************/
/*	  AddData() Add an element of data to a list		  */
/************************************************************************/
void AddData(base, data, writeit, killdups)
SListBase *base;
void		*data;
char		killdups;
void		(*writeit)(void *data);
{
	SListData *rover, *rover2, *trail = NULL;

	if (killdups)
		KillData(base, data);

	rover = (SListData *) (*slistmalloc)(sizeof *rover);

	if (base->start == NULL) {
		base->start = rover;
		rover->next = NULL;
	}
	else {
		for (rover2 = base->start;
		       rover2 != NULL &&
		      (base->cmp == NULL ||
		      (*base->cmp)(rover2->data, data) < 0) ;
				rover2 = rover2->next)
			trail = rover2;

		if (trail == NULL) {
			rover->next = base->start;
			base->start = rover;
		}
		else {
			rover->next = trail->next;
			trail->next = rover;
		}
	}

	rover->data = data;

	if (writeit != NULL)
		RunList(base, writeit);
}

/************************************************************************/
/*	  AltKillData() remove an element of data from a list		*/
/************************************************************************/
void AltKillData(base, check, data)
SListBase *base;
void	*data;
void    *(*check)();
{
	SListData *rover, *trail, *moo;

	trail = base->start;
	rover = base->start;

	while (rover != NULL) {
		if ((*check)(rover->data, data) != NULL) {
			moo = rover->next;
			(*base->FreeFunc)(rover->data);
			(*slistfree)(rover);
			if (trail == rover)
				rover = trail = base->start = moo;
			else
				trail->next = rover = moo;
		}
		else {
			trail = rover;
			rover = rover->next;
		}
	}
}

void KillData(base, data)
SListBase *base;
void	*data;
{
	if (base->CheckIt == NULL) return;

	AltKillData(base, base->CheckIt, data);
}

/************************************************************************/
/*	  KillList() Destroys a list				  */
/************************************************************************/
void KillList(base)
SListBase *base;
{
	SListData *rover, *b;

	for (b = base->start; b != NULL; ) {
		rover = b->next;
		(*base->FreeFunc)(b->data);
		(*slistfree)(b);
		b = rover;
	}

	base->start = NULL;
}

/*
 *	  MaybeKillList() Destroys a list
 */
void MaybeKillList(base, func)
SListBase *base;
int (*func)();
{
        SListData *rover, *b, *trail;

        for (b = base->start, trail = NULL; b != NULL; ) {
                rover = b->next;
                if ((*func)(b->data)) {
                        (*base->FreeFunc)(b->data);
                        if (b == base->start) base->start = rover;
                        if (trail != NULL) trail->next = rover;
			(*slistfree)(b);
                }
                else trail = b;
                b = rover;
        }
}

/************************************************************************/
/*	  SearchList() search a list for a given data element	 */
/************************************************************************/
void *SearchList(base, data)
SListBase *base;
void	*data;
{
	SListData *rover;
	void	 *temp;

	for (rover = base->start; rover != NULL; rover = rover->next)
		if ((temp = (*base->CheckIt)(rover->data, data)) != NULL)
			return temp;

	return NULL;
}

void *AltSearchList(SListBase *base, void *(*doit)(), void *data)
{
	SListData *rover;
	void	 *temp;

	for (rover = base->start; rover != NULL; rover = rover->next)
		if ((temp = (*doit)(rover->data, data)) != NULL)
			return temp;

	return NULL;
}

/*
 * RunList()
 *
 * run a function on a list.
 */
int RunList(base, doit)
SListBase *base;
void	(*doit)(void *data);
{
	SListData *rover;
	int       count = 0;

	for (rover = base->start; rover != NULL; rover = rover->next, count++)
		(*doit)(rover->data);

	return count;
}

/*
 * RunListA()
 *
 * run a function on a list
 */
int RunListA(base, doit, arg)
SListBase *base;
void	(*doit)(void *data, void *rg), *arg;
{
	SListData *rover;
	int       count = 0;

	for (rover = base->start; rover != NULL; rover = rover->next, count++)
		(*doit)(rover->data, arg);

	return count;
}

/*
 * void FrontToEnd(SListBase *base)
 *
 * This function takes the first element of the list, if it exists, and makes 
 * it the last element of the list.  This is useful for lists whose sort order 
 * changes due to outside forces.
 */
void FrontToEnd(base)
SListBase *base;
{
	SListData *r1, *r2;

	if (base->start != NULL && base->start->next != NULL) {
		r1 = base->start;
		base->start = r1->next;
		for (r2 = r1; r2->next != NULL; r2 = r2->next)
			;

		r2->next = r1;
		r1->next = NULL;
	}
}

void NoFree(void *d)
{
}

/*
 * void *GetLast(SListBase *base)
 *
 * This function finds the last element in the list and returns it.  It
 * returns NULL if the list is empty.
 */
void *GetLast(SListBase *base)
{
	SListData *rover;

	if (base->start == NULL) return NULL;
	for (rover = base->start; rover->next != NULL; rover = rover->next)
		;
	return rover->data;
}

