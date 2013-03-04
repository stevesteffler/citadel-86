/*
 *				sysibm.c
 *
 * Contains IBM-specific code.
 */

/*
 *				history
 *
 * 92Oct23 HAW  Rewritten to rid ourselves of the interpreter.
 * 89Jan12 HAW  Created from SysDep2.c.
 */

#define SYSTEM_DEPENDENT
#define TIMER_FUNCTIONS_NEEDED

/* #define HAVE_LSS    -- long lost file - not available from Hue, Jr. */

#include "ctdl.h"
#include "sys\stat.h"
#include "ctype.h"
#include "stdarg.h"
#include <alloc.h>

#ifdef HAVE_LSS
#include <lss.h>
#endif

/*
 *				Contents
 *
 *		MODEM HANDLING:
 *	comint()		handles IBM interrupt
 *	MIReady()		check MS-DOS interrupt for data
 *	waitPutch()		bottom-level modem output
 *	fastMod()		output without waiting
 *	ReInitModem()		reinitialize for recalcitrant modems
 *	setInterrupts()		sets interrupts (IBM only)
 */

extern CONFIG cfg;
#define INTA00		0x20
#define INTA01		0x21		/* Interrupt controller */

#define COMBUF_SIZE     1200
#define EOI		0x20		/* End of Interrupt     */

#define SETDISK		14

static char FirstInit = TRUE;

extern char straight;
extern char onConsole;

static unsigned int ModemCtrl;
/*
 * Section 3.1. MODEM HANDLING:
 *	These functions are responsible for handling modem I/O.
 */

static intSet = FALSE;

static unsigned char old_inta01;
unsigned char buffin[COMBUF_SIZE];
static int cptr, head;

/*
 * comint()
 *
 * This function handles the IBM interrupt.
 */
void interrupt comint()
{
    unsigned char cbuf;
    unsigned int  lsr_data;

    lsr_data = inportb(cfg.DepData.line_status);
    if ((lsr_data & 0x01) == 0) {
	outportb(cfg.DepData.mdm_ctrl, ModemCtrl);
	outportb(INTA00, EOI);
	return;
    }
    cbuf = inportb(cfg.DepData.modem_data);
    if (cptr >= COMBUF_SIZE)
	cptr = 0;
    buffin[cptr++] = cbuf;
    outportb(cfg.DepData.mdm_ctrl, ModemCtrl);
    outportb(INTA00, EOI);
    return ;
}

/*
 * mGetch()
 *
 * This function reads data from the port.  Should not be called if there is
 * no data present (for good reason).
 */
int mGetch()
{
    int k;

    k = cptr;
    if (k==head)
	return 0;
    if (k>head)
	return (buffin[head++]);
    if (head < COMBUF_SIZE)
	return (buffin[head++]);
    head = 0;
    if (head < k)
	return (buffin[head++]);
    printf("ERROR\n");
    return 0;
}

/*
 * mHasch()
 *
 * This function ostensibly checks to see if input from modem ready.
 */
int mHasch()
{
    return (cptr != head);
}

void interrupt (*com_addr)();

/*
 * mClose()
 *
 * This is responsible for shutting down I/O.  For IBMs, interrupts must be
 * disabled; dropping of DTR is impossible otherwise, for some reason unknown
 * to me.    HAW
 * 88Dec16: modified to drop carrier only if need be.  HAW
 */
void mClose()
{
    if (intSet) {   /* kludgeykludgeykludgeykludgey */
	outportb(INTA01, old_inta01);       /* Kill vector */
	setvect(cfg.DepData.com_vector, com_addr);
    }
}

/*
 * waitPutch()
 *
 * This stuffs a char out the modem port.
 */
int waitPutch(int c)
{
        extern char oldmodem;
	int flag;
	unsigned count;
	extern char inNet;
	TimePacket timeout;

	outportb(cfg.DepData.mdm_ctrl, 0x0b);

	/* 120.683 */
	if (!oldmodem) {
		/* 119.674 */
		count = 0;
		setTimer(&timeout);
	}
wait1:
	flag = inportb(cfg.DepData.line_status);

	/* 120.683 */
	if (!oldmodem) {
		/* 119.674 */
		count++;
		if (count % 1000) { /* every 10000 cycles do a timeout check */
			if (timeSince(&timeout) > 60 /* seconds */) {
				if (inNet != NON_NET)
					killConnection("wp");
				printf("Timeout in waitPutch, other side unresponsive.\n");
				return FALSE;
			}
		}
	}

	if ((flag & 0x20) == 0) goto wait1;
	if (cfg.DepData.LockPort != -1) {
	    	flag = inportb(cfg.DepData.modem_status);
	    	if (!(flag & 16)) goto wait1;
	}

	outportb(cfg.DepData.modem_data, c);
	return TRUE;
}

/*
 * fastMod()
 *
 * This function stuffs a char out the modem port without waiting.
 */
char fastMod(int c)
{
    outMod(c);
    return TRUE;
}

/*
 * ModemOpen()
 *
 * This function opens the modem for the first time.
 */
void ModemOpen(char FromDoor)
{
    setInterrupts();
    ModemCtrl = 0x0b;

    if (!FromDoor && FirstInit) {
	InitPort();
	pause(5);
    }
    if (FirstInit) ReInitModem();
    FirstInit = FALSE;
}

/*
#define ANDI            1
#define INP             2
#define LOAD            3
#define LOADI           4
#define ORI             5
#define OUTP            6
#define OUTSTRING       7
#define PAUSEI          8
#define RET             9
#define STORE          10
#define XORI           11
#define STOREX         12
#define LOADX          13
#define OPRNUMBER      14
#define TOBDC          15
#define TODEC          16
 { { 4, 0x83, 6, 0xFB, 3, 4, 0x80, 6, 0xF8, 3, 4, 1, 6, 0xF9, 3, 4, 1,
	6, 0xFC, 3, 4, 3, 6, 0xFB, 3, 7 }, 26 },
LOADI  0x83
OUTP   3FB
LOADI  0x80
OUTP   3F8
LOADI  1
OUTP   3F9
LOADI  1
OUTP   3FC
LOADI  3
OUTP   3FB
OUTSTRING ...
RET
*/
void InitPort()
{

    outportb(cfg.DepData.ln_ctrl, 0x83);
    outportb(cfg.DepData.modem_data, 0x80);
    outportb(cfg.DepData.ier, 1);
    outportb(cfg.DepData.mdm_ctrl, 1);
    outportb(cfg.DepData.ln_ctrl, 3);
    if (cfg.DepData.LockPort >= 0)
	SetBaudTo(cfg.DepData.LockPort);
    pause(10);
    OutString(cfg.codeBuf + cfg.DepData.pInitString);
}

/*
 * setInterrupts()
 *
 * This function sets up IBM interrupts.
 */
static void setInterrupts()
{
    unsigned char bite;

    intSet = TRUE ;
    outportb(cfg.DepData.mdm_ctrl, 0x0b);
    inportb(cfg.DepData.modem_status);
    inportb(cfg.DepData.line_status);
    inportb(cfg.DepData.modem_data);
    bite = inportb(cfg.DepData.ln_ctrl);
    bite &= 0x7f;
    outportb(cfg.DepData.ln_ctrl, bite);
    outportb(cfg.DepData.ier, 0x01);
    old_inta01 = bite = inportb(INTA01);
    bite &= cfg.DepData.PIC_mask;
    outportb(INTA01, bite);
    cptr = head = 0;
    com_addr = getvect(cfg.DepData.com_vector);
    setvect(cfg.DepData.com_vector, comint);
}

/*
 * gotCarrier()
 *
 * This function decides if carrier is present or not.
 */
int gotCarrier()
{
    unsigned int bite;

    bite = inportb(cfg.DepData.modem_status);
    return (bite & 0x80);
}

/*
 * Reinitialize()
 *
 * This function will reinitialize the modem.
 */
void Reinitialize()
{
    InitPort();
}

/*
 * InternalEnDis()
 *
 * This function handles enable/disable of the modem port*.
 */
void InternalEnDis(char enable)
{
    if (enable) {
/* { { LOADX, 0, ORI,  0x01, OUTP, 0xFC, 3, STOREX, 0, RET }, 10 }, */
	ModemCtrl |= 1;
	outportb(cfg.DepData.mdm_ctrl, ModemCtrl);
    }
    else {
	ModemCtrl &= 0xfe;
	outportb(cfg.DepData.mdm_ctrl, ModemCtrl);
    }
}

void mHangUp()
{
    outportb(cfg.DepData.mdm_ctrl, ModemCtrl & 0xFE);
    pause(50);
    outportb(cfg.DepData.mdm_ctrl, ModemCtrl);
}

void SetBaudTo(int x)
{
    static struct BaudInfo {
	int latch1;
	int latch2;
    } baud[] = {
	{ 0x80, 1 },		/* 300  */
	{ 0x60, 0 },		/* 1200 */
	{ 0x30, 0 },		/* 2400 */
	{ 0x18, 0 },		/* 4800 */
	{ 0x0c, 0 },		/* 9600 */
	{ 0x08, 0 },		/* 14,400 */
	{ 0x06, 0 },		/* 19,200 */
	{ 0x03, 0 },		/* 38,400 */
	{ 0x02, 0 },		/* 56,800 */
    };

    outportb(cfg.DepData.ln_ctrl, 0x83);
    outportb(cfg.DepData.modem_data, baud[x].latch1);
    outportb(cfg.DepData.ier, baud[x].latch2);
    outportb(cfg.DepData.mdm_ctrl, 1);
    outportb(cfg.DepData.ln_ctrl, 0x03);
}

extern ScreenMap *ScrColors;
/*
 *	Video and miscellanea
 */

/*
 * StopVideo()
 *
 * This will turn off video as needed.
 */
void StopVideo()
{
}

int SystemPort;
/*
 * CheckSystem()
 *
 * This performs a check on the system to be sure we're not mixing
 * executables on the system, i.e., trying to run the IBM executable on a
 * Z-100.
 */
char CheckSystem()
{
    if (!cfg.DepData.IBM) {
	cfg.DepData.OldVideo = TRUE;
	printf("This .EXE for PClones only.\n");
    }
    else
	ScrColors = &cfg.DepData.ScreenColors;

    SystemPort = (cfg.DepData.modem_data == 0x3f8) ? 1 :
		(cfg.DepData.modem_data == 0x2f8) ? 2 :
		(cfg.DepData.modem_data == 0x3e8) ? 3 : 4;

    return cfg.DepData.IBM;
}

/*
 * RegisterSysopMenu()
 *
 * This will set up for doing special sysop menus.
 */
MenuId RegisterSysopMenu(char *MenuName, char *Opts[], char *MenuTitle,
							int flags)
{
#ifdef HAVE_LSS
	MenuList *data;

	if (onConsole && !cfg.DepData.OldVideo) {
		data = GetDynamic(sizeof *data);
		data->MenuTitle = MenuTitle;
		data->Data = Opts;
		return OpenMenuWindow(data, cfg.DepData.ScreenColors.StatFore, 
				    cfg.DepData.ScreenColors.StatBack);
	}
	else {
		RegisterThisMenu(MenuName, Opts);
		if (flags & SHOW_MENU_OLD) {
			mPrintf("%s", MenuTitle);
			printHelp(MenuName, HELP_BITCH | HELP_NO_HELP);
		}
		return 0;
	}
#else
	RegisterThisMenu(MenuName, Opts);
	return 0;
#endif
}

/*
 * GetSysopMenuChar()
 *
 * This gets a selection from the sysop.
 */
int GetSysopMenuChar(MenuId id)
{
#ifdef HAVE_LSS
    if (onConsole && id != ERROR && !cfg.DepData.OldVideo) {
	return GetMenuSelection(id);
    }
    else return GetMenuChar();
#else
    return GetMenuChar();
#endif
}

/*
 * CloseSysopMenu()
 *
 * This closes the sysop's menu.
 */
void CloseSysopMenu(MenuId id)
{
#ifdef HAVE_LSS
    if (onConsole && id != ERROR && !cfg.DepData.OldVideo) {
	CloseMenuWindow(id);
	RestoreMasterWindow();
    }
#endif
}

/*
 * SysopMenuPrompt()
 *
 * This does prompt handling in sysop menu.
 */
void SysopMenuPrompt(MenuId id, char *prompt)
{
#ifdef HAVE_LSS
    if (cfg.DepData.OldVideo || !onConsole || id == ERROR)
	mPrintf(prompt);
#else
    mPrintf(prompt);
#endif
}

/*
 * SysopError()
 *
 * This will show an error to the sysop.
 */
void SysopError(MenuId id, char *prompt)
{
#ifdef HAVE_LSS
    if (onConsole && !cfg.DepData.OldVideo)
	ErrorReport(id, prompt);
    else mPrintf(prompt);
#else
    mPrintf(prompt);
#endif
}

/*
 * SysopGetYesNo()
 *
 * This does a get yes/no from sysop.
 */
char SysopGetYesNo(MenuId id, char *info, char *prompt)
{
#ifdef HAVE_LSS
    if (!onConsole || cfg.DepData.OldVideo) {
	mPrintf(info);
	return getYesNo(prompt);
    }
    else return MenuGetYesNo(id, info, prompt);
#else
    mPrintf(info);
    return getYesNo(prompt);
#endif
}

/*
 * SysopInfoReport()
 *
 * This will report information to sysop.
 */
void SysopInfoReport(MenuId id, char *info)
{
#ifdef HAVE_LSS
    if (onConsole && !cfg.DepData.OldVideo)
	InfoReport(id, info);
    else mPrintf(info);
#else
    mPrintf(info);
#endif
}

/*
 * SysopRequestString()
 *
 * This gets a string from the sysop.
 */
void SysopRequestString(MenuId id, char *prompt, char *buf, int size, int flags)
{
#ifdef HAVE_LSS
    int jid, width;

    if (onConsole && !cfg.DepData.OldVideo) {
	width = size + strLen(prompt);
	if (width + 6 > 78)
	    width = strLen(prompt);
	jid = ContinualIOWindow("", "", 6, 4, 74, 3,
					cfg.DepData.ScreenColors.StatFore, 
					cfg.DepData.ScreenColors.StatBack);
	SysopPrintf(jid, "\n");
	CIORequestString(jid, prompt, buf, size, flags);
	CloseMenuWindow(jid);
	NormStr(buf);
    }
    else
	getNormStr(prompt, buf, size, flags);
#else
    getNormStr(prompt, buf, size, flags);
#endif
}

/*
 * SysopDisplayInfo()
 *
 * This will display info to the sysop.
 */
void SysopDisplayInfo(MenuId id, char *info, char *title)
{
#ifdef HAVE_LSS
    int newid, count=1;
    char *l, *eol;

    if (onConsole && !cfg.DepData.OldVideo) {
	l = info;
	while ((eol = strchr(l, '\n')) != NULL) {
		l = eol+1;
		count++;
	}
	newid = ContinualIOWindow(title, info, 10, 8, 70, count,
					cfg.DepData.ScreenColors.StatFore, 
					cfg.DepData.ScreenColors.StatBack);
	getch();
	CloseMenuWindow(newid);
    }
    else
	mPrintf("%s", info);
#else
    mPrintf("%s", info);
#endif
}

/*
 * SysopGetNumber()
 *
 * This will get a number from sysop.
 */
long SysopGetNumber(MenuId id, char *prompt, long bottom, long top)
{
#ifdef HAVE_LSS
    char buf[20];
    long try;

    if (onConsole && !cfg.DepData.OldVideo) {
	while (TRUE) {
	    SysopRequestString(id, prompt, buf, sizeof buf, 0);
	    try     = atol(buf);
	    if (try < bottom) SysopError(id, "Try again.");
	    else if (try > top   ) SysopError(id, "Try again.");
	    else break;
	}
	return try;
    }
    else
	return getNumber(prompt, bottom, top);
#else
    return getNumber(prompt, bottom, top);
#endif
}

/*
 * SysopContinual()
 *
 * This sets up a continual I/O window for further use by other code.  It
 * returns a MenuId for it.
 */
MenuId SysopContinual(char *title, char *prompt, int MaxWidth, int Depth)
{
#ifdef HAVE_LSS
    if (onConsole && !cfg.DepData.OldVideo)
	return ContinualIOWindow(title, prompt, 
				40 - (MaxWidth / 2), 5, 40 + (MaxWidth / 2),
				Depth, cfg.DepData.ScreenColors.StatFore, 
				cfg.DepData.ScreenColors.StatBack);
    else {
	mPrintf(prompt);
	return -1;
    }
#else
    mPrintf(prompt);
    return -1;
#endif
}

/*
 * SysopContinualString()
 *
 * This gets a string from the sysop on a continual I/O window.
 */
char SysopContinualString(MenuId id, char *prompt, char *buf, int size, int flags)
{
#ifdef HAVE_LSS
    extern char exChar;

    if (onConsole && id != NO_MENU && !cfg.DepData.OldVideo) {
	CIORequestString(id, prompt, buf, size, flags);
	if (*buf != exChar) NormStr(buf);
	return TRUE;
    }
    else {
	return getNormStr(prompt, buf, size, flags);
    }
#else
    getNormStr(prompt, buf, size, flags);
#endif
}

/*
 * SysopPrintf()
 *
 * This formats format+args to sysop window.
 */
int SysopPrintf(MenuId id, char *format, ...)
{
    va_list argptr;
    char localgarp[2000];
    extern char *garp;

    if (garp == NULL) garp = localgarp;
    va_start(argptr, format);
    vsprintf(garp, format, argptr);
    va_end(argptr);
#ifdef HAVE_LSS
    if (onConsole && id != NO_MENU && !cfg.DepData.OldVideo)
	ToWindow(id, garp);
    else mFormat(garp, oChar, doCR);
#else
    mFormat(garp, oChar, doCR);
#endif
    return 0;
}

/*
 * SysopCloseContinual()
 *
 * This closes a sysop continual I/O window.
 */
void SysopCloseContinual(MenuId id)
{
#ifdef HAVE_LSS
    CloseSysopMenu(id);
#endif
}

char SysopGetChar(MenuId id)
{
#ifdef HAVE_LSS
    if (onConsole && id != NO_MENU && !cfg.DepData.OldVideo) {
	return getch();
    }
    else return modIn();
#endif
}

#ifdef WANTED
/*
 * changeBauds()
 *
 * Change the baud rate according to what the sysop asks for.
 */
int changeBauds(MenuId id)
{
    interpret(cfg.DepData.pBauds[(int) SysopGetNumber(id, " baud (0-6)", 0l, 6l)]);
    return 0;
}
#endif
