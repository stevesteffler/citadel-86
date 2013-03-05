/*
 *				systi.c
 *
 * Contains code to interface to Robert Nelson's TI PC interface.
 */

/*
 *				history
 *
 * 90Nov17 HAW  Created.
 */

#include "ctdl.h"

/*
 *				Contents
 *
 *	ModemOpen()		Opens the modem.
 */

extern CONFIG cfg;
int    SystemPort;

/*
 * ModemOpen()
 *
 * This function opens the modem, TI style.
 */
void ModemOpen(char FromDoor)
{
    TiModemOpen(FromDoor, cfg.DepData.TiInternal, cfg.DepData.TiComPort,
	cfg.sysBaud, cfg.codeBuf + cfg.DepData.pInitPort, cfg.DepData.LockPort,
        cfg.DepData.HiSpeedInit, cfg.DepData.sEnable, cfg.DepData.sDisable);
    SystemPort = cfg.DepData.TiComPort;
}

/*
 * CheckSystem()
 *
 * Are we setup correctly?
 */
char CheckSystem()
{
    return TRUE;
}

/*
 * getCh()
 *
 * This gets char from console.  We do not use a para. define here because we
 * must have a pointer to function elsewhere, and the way we do it here should
 * save a bit of space.
 */
int getCh()
{
    return getch();
}

/*
 * The balance of the functions here are simply barely functional stubs.
 */
MenuId RegisterSysopMenu(char *MenuName, char *Opts[], char *MenuTitle, int flags)
{
	RegisterThisMenu(MenuName, Opts);
	return NO_MENU;
}

int GetSysopMenuChar(MenuId id)
{
	return GetMenuChar();
}

void CloseSysopMenu(MenuId id)
{
}

void SysopMenuPrompt(MenuId id, char *prompt)
{
	mPrintf(prompt);
}

void SysopError(MenuId id, char *prompt)
{
	mPrintf(prompt);
}

char SysopGetYesNo(MenuId id, char *info, char *prompt)
{
	return getYesNo(prompt);
}

void SysopRequestString(MenuId id, char *prompt, char *buf, int size, int flags)
{
	getNormStr(prompt, buf, size, flags);
}

void SysopInfoReport(MenuId id, char *info)
{
	mPrintf(info);
}

void SysopDisplayInfo(MenuId id, char *info, char *title)
{
	mPrintf(info);
}

long SysopGetNumber(MenuId id, char *prompt, long bottom, long top)
{
	return getNumber(prompt, bottom, top);
}

MenuId SysopContinual(char *title, char *prompt, int Width)
{
	mPrintf(prompt);
	return NO_MENU;
}

void SysopContinualString(MenuId id, char *prompt, char *buf, int size)
{
	getNormStr(prompt, buf, size, 0);
}

void SysopCloseContinual(MenuId id)
{
}

int  SysopPrintf(MenuId id, char *format, ...)
{
    va_list argptr;
    extern char *garp;
    char localgarp[2000];

    if (garp == NULL) garp = localgarp;
    va_start(argptr, format);
    vsprintf(garp, format, argptr);
    va_end(argptr);
    mFormat(garp, oChar, doCR);
}
