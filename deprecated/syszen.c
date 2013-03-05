/*
 *				syszen.c
 *
 * This contains some of the Z-100 specific code.
 */

/*
 *				history
 *
 * 89Jan12 HAW  Created from SysDep2.c.
 */

#define SYSTEM_DEPENDENT

#include "ctdl.h"
#include "sys\stat.h"
#include "ctype.h"

int SystemPort = 0;

/*
 *				Contents
 *	
 *	rawModemInit()		does raw initialization of modem
 *	statusline()		update statusline
 *	video()			initializes video
 */

extern CONFIG cfg;

extern char NewVideo = FALSE;
extern char InvVideo = FALSE;

extern char *HiSpeedInit = NULL;

static char Refresh = 0;

static char FirstInit = TRUE;

extern char straight;

/*
 * Section 3.1. MODEM HANDLING:
 *	These functions are responsible for handling modem I/O.
 */

/*
 * ModemOpen()
 *
 * This initializes the modem in a system dependent manner.
 */
void ModemOpen(char FromDoor)
{
    static int Zbaud = 1200;

    mInit(2, Zbaud, 'N', 1, 8, 0);
    Zbaud = 0;
    if (FirstInit) cfg.scratch[0] = inportb(0xEF);

    if (!FromDoor && FirstInit) ReInitModem();
    FirstInit = FALSE;
}

/*
 * waitPutch()
 *
 * This stuffs a char out the modem port.
 */
int waitPutch(int c)
{
    mPutch(c);
    while (mHasout())
	;
    return TRUE;
}

/*
 * fastMod()
 *
 * This will shove and run.
 */
char fastMod(int c)
{
    mPutch(c);
    return TRUE;
}

#define SAVE_CUR	"\033j"
#define RET_CUR		"\033k"
#define ENABLE_25	"\033x1"
#define DISABLE_25	"\033y1"
#define GOTOXY		"\033Y"
#define REV_VID		"\033p"
#define NORM_VID	"\033q"
#define ERASE_EOL	"\033K"

static int tag_len;

/*
 * video()
 *
 * This initializes video.
 */
void video(char *tagline)
{
    straight = TRUE;
    printf("%s%s%s%c%c%s%-80s%s%s",
	SAVE_CUR, ENABLE_25, GOTOXY, 24+32, 32, (InvVideo) ? REV_VID : "",
	tagline, NORM_VID, RET_CUR);
    tag_len = strLen(tagline);
    straight = FALSE;
}

/*
 * statusline()
 *
 * This will update statusline.
 */
void statusline(char *data)
{
    straight = TRUE;
    printf("%s%s%c%c%s%-*s%s%s",
	SAVE_CUR, GOTOXY, 24+32, 32 + tag_len, (InvVideo) ? REV_VID : "",
	80-tag_len, data, NORM_VID, RET_CUR);
    straight = FALSE;
}

/*
 * StopVideo()
 *
 * This will stop video handling.
 */
void StopVideo()
{
    straight = TRUE;
    printf(DISABLE_25);
    straight = FALSE;
}

/*
 * vputch()
 *
 * This will place a character on the screen.
 */
char vputch(unsigned char c)
{
    DoBdos(6, c);
}

/*
 * KeyStroke()
 *
 * This will get a character from console.
 */
int KeyStroke(void)
{
    return (DoBdos(7, 0) & 0xff);
}

/*
 * CheckSystem()
 *
 * This does some initial setup stuff.
 */
char CheckSystem()
{
    printf("Z-100\n");
    return !cfg.DepData.IBM;
}

/*
 * changeBauds()
 *
 * Change the baud rate according to what the sysop asks for.
 */
int changeBauds(MenuId id)
{
    interpret(cfg.DepData.pBauds[(int) getNumber(" baud (0-6)", 0l, 5l)]);
}
