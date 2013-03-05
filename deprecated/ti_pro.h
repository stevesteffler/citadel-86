/*
 *  (c) Copyright Robert Nelson (The Grey Knight), Saint Paul, MN. 1990,1991.
 *  All rights are hereby released to the public domain.  The author shall
 *  not be liable for damages arising from the use or inability to use
 *  these functions. R.Nelson 11-27-90
 */

/* A sample MAKEFILE and linker command file are included at the end
 * of this file.
 *
 * Warning:  Turbo C's delay() function tries to write to a timer that is not
 * present on the TI Pro.  This will lock up a TI Pro, requiring powering
 * down and restarting.  Use the delay function in TI_DELAY.C / TI_DELAY.OBJ.
 * TI_DELAY.LIB must appear first on the Turbo C or MAKE command line.  If
 * rearranging the code, make sure that ti_delay() or your own delay() function,
 * or the C file containing the delay code appears first on the TC command
 * line, or the linker may link in Turbo C's delay() function with CTDL.OBJ.
 */


/******************************************************************************/
/*                include file for TI_MODEM.C and TI_VIDEO.C                  */
/******************************************************************************/


/*                                 defines                                    */

#define CRASH_EXIT      2  /* ERRORLEVEL crash code to return to DOS     */
                           /* consistent with Citadel's error exit code. */

#define TI_INTA00    0x18  /* TI 8259 Interrupt    */
#define TI_INTA01    0x19  /* controller addresses */

#define EOI          0x20  /* End of Interrupt     */

#define COMBUF_SIZE  9600  /* Size of the receive buffer */
                           /* if made larger than 32767, you must locate and */
                           /* resolve all references, since COMBUF_SIZE      */
                           /* is used as an integer value in the code below. */

/* perhaps better to check for sysbaud = 14400 or 19200, and malloc() the
   buffer, instead of using COMBUF_SIZE... */

#define BLACK           0
#define TRUE            1
#define FALSE           0
#define BUSY         (-1)  /* RottenDial() returns this if phone is busy */





/*                         modem function prototypes                          */

extern void DisableModem(char FromNet);
extern void EnableModem(char FromNet);
extern void HangUp(char FromNet);
extern void Reinitialize(void);
extern void setTIbaudrate(int speed);
extern void writeTIport(int rgster,unsigned char value);
extern void BufferingOn(void);
extern void BufferingOff(void);
extern void TiModemOpen(char FromDoor, char internal, int portnumber,
                  int sysbaudrate, char *ModemInitString, int LockPortAtBaud,
                  char *HiSpeedInitString, char *EnableString,
                  char *DisableString);
extern void ModemShutdown(char KillCarr);
extern void ReInitModem(void);
extern void interrupt comint();
extern void TImoPuts(char *s);
extern void ModemPushBack(char c);
extern int  setNetCallBaud(int targetBaudCode);
extern int  gotCarrier(void);
extern int  MIReady(void);
extern int  RottenDial(char *callout_string);
extern char outMod(int c);
extern char fastMod(int c);
extern char TIgetMod(void);
extern unsigned char Citinp(void);


/*                         video function prototypes                          */

extern int  changeBauds(void);   /* uses ti_modem.c global variable sysbaud */

extern void StopVideo(void);
extern void video ( char *statustitle );
extern void vlocate ( unsigned char row, unsigned char col );
extern void vputs ( char *string );
extern void vputch ( char c );
extern void statusline ( char *work );

/* delay function prototype */

void delay(unsigned milliseconds);


/*                              end of ti_pro.h                               */

/*
This is the MAKEFILE in it's entirety:
_________________________________________
CC=tcc -w -ml -G -O -Z -d
INC=-IF:\TC\INCLUDE
HEADERS=ti_pro.h

.c.obj:
   $(CC) -c $(INC) $<

NORMAL: ctdl-ti.exe

ctdl-ti.exe: ti_modem.obj ti_video.obj

     tlink @tilink
_________________________________________



This is the linker file, TILINK, in it's entirety:

_________________________________________
/c /s f:\tc\lib\c0l ti_modem ti_video netmisc systi CTDL
ctdl-ti

ti_delay most f:\tc\lib\emu f:\tc\lib\mathl f:\tc\lib\cl
_________________________________________
*/

