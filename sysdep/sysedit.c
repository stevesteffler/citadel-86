/*
 *				sysedit.c
 *
 * Message Editor facilities.
 */

/*
 *				history
 *
 * 89Oct18 HAW  Created.
 */

#define SYSTEM_DEPENDENT

#ifndef NO_EXTERNAL_EDITORS

#include "ctdl.h"

/*
 *				Contents
 *
 *	OutsideEditor()		use an outside editor for sysop.
 *	OutsideEditorWork()	work fn to execute outside editor.
 *	InitExternEditors()	initializes external editors.
 *	EatEditorLine()		eats a line from editors.sys.
 *	OtherEditOptions()	adds options to editing menu.
 *	ShowOutsideEditors()	shows external editors somewhere.
 *	DisplayEditOpts()	displays editing options work fn.
 *	AddOurEditOpts()	work fn for adding editing options.
 *	FindEditorSelector()	find the editor depending on selector.
 *	RunRemoteEditor()	runs any extended editor.
 */

typedef struct {
    char *Name;
    char *CmdLine;
} Editor;

void *EatEditorLine(char *line);
void *FindEditorSelector();

static SListBase ExtEditors = { NULL, FindEditorSelector, NULL, NULL,
							EatEditorLine };

extern CONFIG  cfg;			/* Lots an lots of variables */
extern MessageBuffer msgBuf;		/* Message buffer */

void OutsideEditorWork(char *EditLine);

/*
 * OutsideEditor()
 *
 * This will let us use an outside editor for sysop.
 */
void OutsideEditor()
{
    OutsideEditorWork(cfg.DepData.Editor);
}

/*
 * OutsideEditorWork()
 *
 * This will execute an outside editor for anyone.
 */
void OutsideEditorWork(char *EditLine)
{
    char cmdline[90];
    extern FILE *upfd;
    extern int outPut;

    doCR();	/* this gets crtColumn back to zero */
    sprintf(cmdline, "%stempmsg.sys", cfg.DepData.EditArea);

    if (!redirect(cmdline, INPLACE_OF)) return;

    mFormat(msgBuf.mbtext, oChar, doCR);
    undirect();

    MakeCmdLine(cmdline, EditLine, "", sizeof cmdline - 1);
    if (cfg.DepData.IBM) ModemShutdown(FALSE);
    CitSystem(TRUE, "%s %stempmsg.sys", cmdline, cfg.DepData.EditArea);
    if (cfg.DepData.IBM) {
	ModemOpen(FALSE);
	if (!gotCarrier() && strLen(cfg.DepData.sDisable) == 0)
	    DisableModem(FALSE);
    }
    /* homeSpace(); */    /* Commented out for 120.692 */
    msgBuf.mbtext[0] = 0;

    sprintf(cmdline, "%stempmsg.sys", cfg.DepData.EditArea);
    msgBuf.mbtext[0] = 0;
    ingestFile(cmdline, msgBuf.mbtext);
    unlink(cmdline);
}

/*
 * InitExternEditors()
 *
 * This is an initialization function for outside editors.
 */
void InitExternEditors()
{
    SYS_FILE fn;

    makeSysName(fn, "editors.sys", &cfg.roomArea);
    MakeList(&ExtEditors, fn, NULL);
}

/*
 * EatEditorLine()
 *
 * This will eat a line from editors.sys and return a variable of type
 * Editor for use in list stuff.
 *
 * format is "<selector letter> <s.name> <command line>"
 */
static void *EatEditorLine(char *line)
{
    Editor *work;
    char   *c;

    if ((c = strchr(line, ' ')) == NULL) return NULL;
    if (line[0] == '"') {
	line++;
	if ((c = strchr(line, '"')) == NULL) return NULL;
	*c = 0;
	c++;
    }
    *c = 0;
    work = GetDynamic(sizeof *work);
    work->Name = strdup(line);
    work->CmdLine = strdup(c + 1);
    return work;
}

/*
 * OtherEditOptions()
 *
 * This function will add options to the editor menu.  This is called by
 * editText().
 */
void OtherEditOptions(char **Options)
{
    void AddOurEditOpts();

    RunListA(&ExtEditors, AddOurEditOpts, (void *) Options);
}

/*
 * ShowOutsideEditors()
 *
 * This will show available external editors.
 */
void ShowOutsideEditors()
{
    void DisplayEditOpts();

    RunList(&ExtEditors, DisplayEditOpts);
}

/*
 * DisplayEditOpts()
 *
 * This is the work fn that actually will display the editing options.
 */
static void DisplayEditOpts(Editor *d)
{
    mPrintf("<%c>%s\n ", d->Name[0], d->Name + 1);
}

/*
 * AddOurEditOpts()
 *
 * This is a work fn to add an external editor option to a list.
 */
static void AddOurEditOpts(Editor *d, char **TheOpts)
{
    ExtraOption(TheOpts, d->Name);
}

/*
 * FindEditorSelector()
 *
 * This function will try to find which editor has been requested.  It is
 * used in list handling.
 */
static void *FindEditorSelector(Editor *d, int *s)
{
    if (d->Name[0] == *s)
	return d;
    return NULL;
}

/*
 * RunRemoteEditor()
 *
 * This function will run any of the extended editors.
 */
void RunRemoteEditor(int s)
{
    Editor *TheEditor;

    if ((TheEditor = SearchList(&ExtEditors, &s)) != NULL)
	OutsideEditorWork(TheEditor->CmdLine);
}

#else

void OutsideEditor()
{
}
void InitExternEditors()
{
}
void OtherEditOptions(char **Options)
{
}
void RunRemoteEditor(int s)
{
}

#endif
