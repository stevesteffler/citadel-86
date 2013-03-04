/*
 * 				sList.h
 * #include file for List.C.
 */

/*
 *		List handling structures - generic sorted lists
 */

#ifndef slist_h

#define slist_h

typedef struct Slist      SListData;
typedef struct sbasestuff SListBase;

typedef void (*ITERATOR)();

#define GetFirst(x)     (((x)->start != NULL) ? (x)->start->data : NULL)
#define MoveAndClear(s, d)	(d)->start = (s)->start, (s)->start = NULL

/*
 * This is the generic list structure normal link.  It contains a pointer
 * to the next element in the list and a pointer to a chunk of data.
 */
struct Slist {
        void     *data;
        SListData *next;
};

/*
 * This structure contains data and functions necessary to handle some given 
 * instantiation of a list.  Included is a pointer to the data of the list, 
 * and pointers to functions which should always be used for given functions 
 * applied to the list.
 */
struct sbasestuff {
        SListData *start;
        void     *(*CheckIt)(void *d1, void *d2);
        int      (*cmp)(void *d1, void *d2);
        void     (*FreeFunc)(void *d);
        void     *(*EatLine)(char *line);
};

#define InitListValues(l, ci, xcmp, f, e)	\
	(l)->start = NULL, (l)->CheckIt = ci,	\
	(l)->cmp = xcmp, (l)->FreeFunc = f, (l)->EatLine = e

char MakeList(SListBase *base, char *fn, FILE *fd);
void AddData(SListBase *base, void *data, void (*writeit)(void *data), 
                                char killdups);
void KillData(SListBase *base, void *data);
void AltKillData(SListBase *base, void *(*check)(), void *data);
void KillList(SListBase *base);
void *GetLast(SListBase *base);
void *SearchList(SListBase *base, void *data);
void *AltSearchList(SListBase *base, void *(*doit)(), void *data);
int  RunList(SListBase *base, void (*doit)(void *data));
int  RunListA(SListBase *base, void (*doit)(void *data, void *rg), void *arg);
void FrontToEnd(SListBase *base);
void NoFree(void *Data);
char *GetAString(char *line, int size, FILE *fd);

extern void *(*slistmalloc)(unsigned size);
extern void  (*slistfree)(void *);
#endif
