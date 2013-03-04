/*
 *				sysZIbm.c
 *
 * This code acts as a central switching point for handling modem stuff in the
 * IBM and Z-100 versions of Citadel-86.  The stuff to handle the upcoming
 * TIPC port is elsewhere.  NB: not all modem calls appear in here; some go
 * directly to the system specific calls in syszen and sysibm.
 */

/*
 *				history
 *
 * 92Oct18 HAW  Killed the interpreter.
 * 90Oct28 HAW  Created.
 */

#define SYSTEM_DEPENDENT
#define TIMER_FUNCTIONS_NEEDED

#include "ctdl.h"
#include "sys\stat.h"
#include "ctype.h"
#include "time.h"

/*
 *				Contents
 *
 *		MODEM HANDLING:
 *	Citinp()		modem input with system overtones
 *	MIReady()		check MS-DOS interrupt for data
 *	outMod()		bottom-level modem output
 *	ReInitModem()		reinitialize for recalcitrant modems
 *	ModemPushBack()		Allow pushbacks
 *	HangUp()		hang up the modem
 *	ModemShutdown()		shut down the modem
 *  #	OutString()		send a string out
 *	DisableModem()		Disable modem
 *	EnableModem()		Enable modem
 *
 *		# == local for this implementation only
 */

#define SETDISK		14

extern CONFIG    cfg;		/* Lots an lots of variables    */

void OutString(char *s);

/*
 * Section 3.1. MODEM HANDLING:
 *	These functions are responsible for handling modem I/O.
 */

static char HasPushBack = FALSE;
static int  PB;
/*
 * Citinp()
 *
 * This reads data from port.  Should not be called if there is no data
 * present (for good reason).
 */
unsigned char Citinp()
{
    if (HasPushBack) {
	HasPushBack = FALSE;
	return (unsigned char) PB;
    }
    return mGetch();
}

/*
 * MIReady()
 *
 * This function ostensibly checks to see if input from modem ready.
 */
int MIReady()
{
    return (HasPushBack || mHasch());
}

/*
 * ModemPushBack()
 *
 * This function allow pushbacks on the modem.
 */
void ModemPushBack(int c)
{
    HasPushBack = TRUE;
    PB = c;
}

/*
 * HangUp()
 *
 * This function will hang up the modem.
 */
void HangUp(char FromNet)
{
    if (!FromNet && cfg.DepData.LockPort >= 0) pause(100);

    mClose();

    mHangUp();

    if (gotCarrier())	/* do it twice if necessary! */
	mHangUp();

    ModemOpen(FALSE);

    ReInitModem();

    while (MIReady()) Citinp();    /* Clear buffer of garbage */
}

/*
 * ModemShutdown()
 *
 * This function will shut down the modem.
 */
void ModemShutdown(char KillCarr)
{
    mClose();
		/* Use user-provided code for here      */
    if (KillCarr) {
	mHangUp();
	DisableModem(TRUE);		/* valid re-use for this port */
    }
}

/*
 * outMod
 *
 * This function stuffs a char out the modem port.
 */
char outMod(int c)
{
    waitPutch(c);
    return TRUE;
}

/*
 * ReInitModem()
 *
 * This function will reinitialize the modem at a high speed.
 */
void ReInitModem()
{
    if (strLen(cfg.DepData.HiSpeedInit) != 0 && !gotCarrier()) {
	setNetCallBaud(cfg.sysBaud);
	OutString(cfg.DepData.HiSpeedInit);
    }
}

/*
 * OutString()
 *
 * This will send a string out.
 */
void OutString(char *s)
{
    char temp;

    while(*s) {
	if (cfg.BoolFlags.debug) printf("%c", *s);
	if (cfg.DepData.InterCharDelay) pause(cfg.DepData.InterCharDelay);
	temp = *s;
	outMod(*s++);    /* output string */
	if (temp == '\r')
	    pause(100);	/* wait a full second */
    }
}

/*
 * EnableModem()
 *
 * This will enable the modem for use.
 */
void EnableModem(char FromNet)
{
    if (FromNet || strLen(cfg.DepData.sEnable) == 0)
	InternalEnDis(TRUE);
    else
	OutString(cfg.DepData.sEnable);
}

/*
 * DisableModem()
 *
 * This function will disable the modem.
 */
void DisableModem(char FromNet)
{
    if (FromNet || strLen(cfg.DepData.sDisable) == 0)
	InternalEnDis(FALSE);
    else
	OutString(cfg.DepData.sDisable);
}

/*
 * setNetCallBaud(int x)
 *
 * This sets the baud rate for the network.
 */
int setNetCallBaud(int x)
{
    if (cfg.DepData.LockPort == -1)
	SetBaudTo(minimum(x, cfg.sysBaud));
}

/*
 * getCh()
 *
 * This function gets char from console.  We do not use a para. define
 * here because we must have a pointer to function elsewhere, and the way we
 * do it here should save a bit of space.
 */
int getCh()
{
#ifdef REALWAY
    return KeyStroke();
#else
int c;

c = KeyStroke();
if (c == F1_KEY) return 0;
return c;
#endif
}

void BufferingOn() { }
void BufferingOff() { }

/* stub */
int RottenDial(char *callout_string)
{
    return FALSE;
}
