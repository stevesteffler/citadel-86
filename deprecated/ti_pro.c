/*  Citadel-86/TI Professional Computer BBS video and modem routines...
 *
 *  (c) Copyright Robert Nelson (The Grey Knight), Saint Paul, MN. 1990.
 *  All rights are hereby released to the public domain.  The author shall
 *  not be liable for damages arising from the use or inability to use
 *  these functions.  R. Nelson, 11/27/90.
 *
 *  This port is dedicated to Hue, Jr. of Test System (Twin Cities) whose
 *  Citadel programming acheivements, effort, and dedication make those of
 *  this work pale in comparison... and make Citadel-86 what it is today.
 *
 *  Thanks also to Cynbe Ru Taren, author of the original CP/M 2.2
 *  Citadel, and all those who contributed to Citadel and Citadel-86.
 *
 *  This file, TI_PRO.C, with TI_PRO.H and TI_DELAY.LIB contain all
 *  the required functions to make Citadel-86 run on the TI Professional
 *  Computer. ( Turbo C's delay() uses IBM hardware not present on the TIPC )
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>

#include "ti_pro.h"  /* the function prototypes are all in this file  */

/* defines */

#define CRASH_EXIT      2
#define TI_INTA00    0x18  /* TI 8259 Interrupt    */
#define TI_INTA01    0x19  /* controller addresses */
#define EOI          0x20  /* End of Interrupt     */
#define COMBUF_SIZE  9600
#define BLACK           0
#define TRUE 1
#define FALSE 0
#define BUSY (-1)

/* globals */

void interrupt (*com_addr)();
static char intSet = FALSE;
static unsigned char combuf[COMBUF_SIZE];
static unsigned char old_inta01;
static int cptr, head;
static unsigned char active_port;
static int sysbaud = 0;
static int TIlockport = 0;
static char internalmodem = 0;
static char ptr_insurance[] = "";
static char *init_string = ptr_insurance;
static char *high_speed_init = ptr_insurance;
static char *enable_string = ptr_insurance;
static char *disable_string = ptr_insurance;


extern int _Cdecl directvideo = 0;

typedef struct {
      int  uart_control;
      int  uart_data;
      int  modem_status;
      int  com_vector;
      int  PIC_mask;
} UARTDATA;

UARTDATA ti_port[4] = {
        {0xe6,0xe7,0xe4,0x40,0xfe},
        {0xee,0xef,0xec,0x41,0xfd},
        {0xf6,0xf7,0xf4,0x42,0xfb},
        {0xfe,0xff,0xfc,0x44,0xef} };
/* uart  ctrl,data,stat,vctr,mask
 * vector 43 is not a com vector ! 40,41,42,44 sequence IS CORRECT!
 */

/* End of modem variables.  The video variables follow... */

static char huge *attrlatch=(char huge *)0xDF800000l;
static char *temp;
static int NORM;
static int CONTRAST;

static unsigned char vatt =0,
           vrow         = 0,
           vcol         = 0,
           vtop         = 0,      /* defaults */
           vbot         = 24,
           vleft        = 0,
           vright       = 79;

/* Modem functions */

/*************************************************************************/
/*  TiModemOpen initializes and enables the 8530 UART and sets variables */
/*  local to this file.                                                  */
/*************************************************************************/
void TiModemOpen(char FromDoor, char internal, int portnumber,
                  int sysbaudrate, char *ModemInitString, int LockPortAtBaud,
                  char *HiSpeedInitString, char *EnableString,
                  char *DisableString)
{
  unsigned char bite;

  directvideo=0;  /* make sure that's shut off, TURBO C's global variable */

  if(strlen(ModemInitString))
     init_string = ModemInitString;
  if(strlen(HiSpeedInitString))
     high_speed_init = HiSpeedInitString;
  if( (strlen(EnableString)) && (strlen(DisableString)) )
  {
     enable_string = EnableString;
     disable_string = DisableString;
  }

  sysbaud = sysbaudrate;
  if(internal) {
     internalmodem = internal;
     sysbaud = (sysbaudrate == 0) ? 0 : 1;
  }
  active_port = portnumber;
  TIlockport = (internalmodem) ? 0 : LockPortAtBaud;
  old_inta01 = bite = inportb(TI_INTA01); /* save the old interrupt */

  if(!FromDoor) {

    writeTIport( 9,(unsigned char)0xC0);   /* 11000000  master hardware reset              */
    writeTIport( 9,(unsigned char)0x40);   /* 01000000  reset channel B                    */
    writeTIport( 4,(unsigned char)0x4E);   /* 01001110  2 stop, no parity, brate gen == 16 */
    writeTIport( 2,(unsigned char)TI_INTA01);    /* TI 8259 interrupt controller address   */
    writeTIport( 3,(unsigned char)0xC0);   /* 11000000  rx enable, 8 bits, no auto enable  */
    writeTIport( 5,(unsigned char)0x60);   /* 01100000  tx enable, 8 bits                  */
    writeTIport( 6,(unsigned char)0x00);   /* 00000000  enable asynch mode                 */
    writeTIport( 7,(unsigned char)0x00);   /* 00000000  enable asynch mode                 */
    writeTIport( 9,(unsigned char)0x01);   /* 00000001  vector includes status             */
    writeTIport(10,(unsigned char)0x00);   /* 00000000  enable asynch mode                 */
    writeTIport(11,(unsigned char)0x56);   /* 01010110  baud rate generator initialize     */
    writeTIport(12,(unsigned char)0xFF);   /* 11111111  low byte of baud rate for 300 baud */
    writeTIport(13,(unsigned char)0x00);   /* 00000000  high byte of baud rate for 300 baud*/

    if(internalmodem)
       setTIbaudrate(sysbaud);
    else
       setTIbaudrate((sysbaud > TIlockport) ? sysbaud : TIlockport);
            /* set to highest speed supported */

    writeTIport(14,(unsigned char)227);    /* 11100011  enable baud rate generator SYSCLOCK*/
                                           /* also sets NRZI mode */
  } /* end of if !FromDoor test */

    writeTIport(15,(unsigned char)0x00);   /* 00000000  turn off all status interrupts     */
/*  set interrupts */
    writeTIport( 3,(unsigned char)0xC1);   /* 11000001  enable receive                     */
    writeTIport( 5,(unsigned char)0xEA);   /* 11101010  enable transmit, raise DTR, RTS    */
/*  enable interrupts */
    writeTIport( 9,(unsigned char)0x09);   /* 00001001  master int enable, vect incl status*/
    writeTIport( 1,(unsigned char)0x10);   /* 00010000  recvd char interrupt enable        */
/* vector the interrupt */
    bite &= (unsigned char)ti_port[active_port].PIC_mask; /* make the new one */
    outportb(TI_INTA01, bite);                            /* and set it */
    cptr = head = 0;                           /* circular queue initialize */
    com_addr = getvect(ti_port[active_port].com_vector);  /* save old vector */
    setvect(ti_port[active_port].com_vector, comint);         /* set new one */
    intSet = TRUE ; /* keep track of whether the com interrupt is revectored */

/* make sure DTR and RTS are raised, probably redundant... */
    writeTIport( 3,(unsigned char)0xC1);   /* 11000001  enable receive                     */
    writeTIport( 5,(unsigned char)0xEA);   /* 11101010  enable transmit, raise DTR, RTS    */
    if(!FromDoor)
       Reinitialize();

}


/******************************************************/
/*      comint() Handles the TI Pro interrupt         */
/******************************************************/
/* done for TI PRO, */
void interrupt comint()
{
    unsigned char cbuf;
    unsigned int  lsr_data;

/* you need the spec sheet on the Intel 8530 UART to be able to
understand this... */

        lsr_data = inportb(ti_port[active_port].uart_control);
        if ((lsr_data & (unsigned int)0x01) == 0) {
            outportb(ti_port[active_port].uart_control, (unsigned char)0xea);
            outportb((int)TI_INTA00, (unsigned char)EOI);
            outportb(ti_port[active_port].uart_control,(unsigned char)0x20);
            return;
        }
        cbuf = inportb(ti_port[active_port].uart_data);
        if (cptr >= COMBUF_SIZE)
            cptr = 0;
        combuf[cptr++] = cbuf; /* add the recvd char to the buffer */
        outportb(ti_port[active_port].uart_control, (unsigned char)0xea);
/* send end of interrupt and re-enable the interrupt for the next one */
        outportb((int)TI_INTA00, (unsigned char)EOI);
        outportb(ti_port[active_port].uart_control,(unsigned char)0x20);
            return;
}

/************************************************************************/
/* ModemPushBack puts a char back into the input buffer, like ungetch() */
/************************************************************************/

void ModemPushBack(char c)
{
     if(head > 0)
        --head;
     else
        head = combuf[COMBUF_SIZE-1];
}

/************************************************************************/
/* ModemShutDown():                                                     */
/* we're bringing CTDL down, put the interrupt vector back where it was */
/* when CTDL was brought up, leave carrier on if a door is involved.    */
/************************************************************************/

void ModemShutdown(char KillCarr)
{

   if (intSet) {    /* Kill vector */

      if(KillCarr)
         DisableModem(TRUE);
      outportb(TI_INTA01, old_inta01);
      setvect(ti_port[active_port].com_vector, com_addr);
      intSet=0;
   }
   else
      if(KillCarr)
         DisableModem(TRUE);
}


/************************************************************************/
/* changeBauds() queries the system operator for the new baud rate to   */
/* set the modem to, and then does so.                                  */
/************************************************************************/
int changeBauds(void)  {
unsigned char ocol = vcol, orow = vrow;
char msg[]=" Space cycles, return selects. Baud rate: ";
char *rates[] = {
        "300", "1200", "2400", "4800", "9600", "14400", "19200", "abort"
} ;
register int select = 0;

      statusline(msg);
      vatt = CONTRAST;
      while(TRUE) {
         vlocate(24,62);
         vputs("     ");
         vlocate(24,62);
         vputs(rates[select]);
         while(!kbhit());
         if(getch() == '\r')
           break;
         select++;
         if(select == 8)
           select = 0;
         if(select > sysbaud)
           select = 7;
      }
      vatt = NORM;
      vlocate ( orow, ocol );
      if(select == 7)
         return(TRUE);
      else {
         setTIbaudrate(select);
         return(TRUE);
      }
}


/********************************/
/* buffering is not implemented */
/*  these functions do nothing. */
/********************************/

void BufferingOn(void) {
  return;
}

void BufferingOff(void) {
  return;
}


/************************************************************************/
/*  gotCarrier() returns FALSE if there is no carrier at the modem port,*/
/*  non-zero if there is carrier.                                       */
/************************************************************************/

int gotCarrier(void) {
char a;

      a = inportb(ti_port[active_port].uart_control);
      a &= 0x08;
      if(a)
         return TRUE;
      else
         return FALSE;

}

/************************************************************************/
/*     writeTIport() writes an internal 8530 UART control register      */
/************************************************************************/

void writeTIport(int rgster,unsigned char value)
{

    if(rgster != 0) outportb(ti_port[active_port].uart_control,
                               (unsigned char)(rgster & 0x0F));
    outportb(ti_port[active_port].uart_control,(unsigned char)value);
}

/************************************************************************/
/*           setTIbaudrate() sets the UART baud rate                    */
/************************************************************************/

void setTIbaudrate(int speed) {
      switch(speed) {
           case  0:  writeTIport(12,(unsigned char)0xFF);    break;    /* set 300 baud */
           case  1:  writeTIport(12,(unsigned char)0x3F);    break;    /* set 1200 baud */
           case  2:  writeTIport(12,(unsigned char)0x1F);    break;    /* set 2400 baud */
           case  3:  writeTIport(12,(unsigned char)0x0E);    break;    /* set 4800 baud */
           case  4:  writeTIport(12,(unsigned char)0x06);    break;    /* set 9600 baud */
           case  5:  writeTIport(12,(unsigned char)0x03);    break;    /* set 14400 baud */
           case  6:  writeTIport(12,(unsigned char)0x02);    break;    /* set 19200 baud */
           default:  vputs(" ERROR: check CTDLCNFG baud rate setting.\n");
                     exit(CRASH_EXIT);
      }
           return;
}


/************************************************************************/
/*      MIReady() Ostensibly checks to see if input from modem ready    */
/************************************************************************/
int MIReady(void)
{
    return (cptr != head);
}


/************************************************************************/
/*      outMod and fastMod stuff a char out the modem port              */
/************************************************************************/
char outMod(int c)
{
        outportb(ti_port[active_port].uart_control, (unsigned char)0x05);
        outportb(ti_port[active_port].uart_control, (unsigned char)0xea);
            while ((inportb(ti_port[active_port].uart_control) & 0x04 ) == 0);
        outportb(ti_port[active_port].uart_data, (unsigned char)c);
        return TRUE;
}
char fastMod(int c)
{
    outMod(c);
    return TRUE;

}

/************************************************************************/
/*      Citinp() reads data from port.  Should not be called if there is   */
/*      no data present (for good reason).                              */
/************************************************************************/
AN_UNSIGNED Citinp(void) {

    int k;

        k = cptr;


        if (k==head)
                return 0;
        if (k>head)
                return (combuf[head++]);
        if (head < COMBUF_SIZE)
                return (combuf[head++]);
        head = 0;
        if (head < k)
                return (combuf[head++]);
        printf("ERROR\n");
        return 0;
}

/************************************************************************/
/* HangUp() hangs the modem up and then re-enables it.  The parameter   */
/* lets you do a final pause, if necessary, on user log outs (otherwise,*/
/* if you do port locking, LONOTICE can be partially lost).             */
/************************************************************************/

void HangUp(char FromNet)  {
    if(!FromNet)
      delay(100);
    writeTIport(5,(unsigned char)0x68); /* write to UART register 5; drop DTR and RTS */
    delay(50);
    ReInitModem();
    writeTIport(5,(unsigned char)0xEA); /* write to UART register 5; raise DTR and RTS */
    if(strlen(high_speed_init))
       TImoPuts(high_speed_init);
    else
    TImoPuts(init_string);
}

/*************************************************************/
/*    Reinitialize() reinitializes the port and the modem    */
/*************************************************************/

void Reinitialize(void) {

    writeTIport(5,(unsigned char)0xEA); /* write to UART register 5; raise DTR and RTS */
    TImoPuts(init_string);
}

/*****************************************************************/
/*    ReInitModem() sets port to the highest system baud rate    */
/*****************************************************************/

void ReInitModem(void)  {
   if(internalmodem)
      setTIbaudrate(sysbaud);
   else
      setTIbaudrate((sysbaud > TIlockport) ? sysbaud : TIlockport);
}


/************************************************************************/
/* DisableModem() this ensures that the modem will not accept incoming  */
/* calls (that is, answer the phone).  This is not necessarily a hang   */
/* up function; it exists to allow the sysop to use his or her system   */
/* without having to manually play games with his modem or phone        */
/* system.  FromNet indicates whether or not the call has to do with    */
/* the network.  If not, you may wish to let the user have a choice     */
/* between your 'normal' disablement procedure and something else (like */
/* forcing the modem offhook).                                          */
/************************************************************************/


/*********************************************************/
/*    DisableModem() drops DTR to disable autoanswer     */
/*********************************************************/

void DisableModem(char FromNet)  {
  if(internalmodem)
  {
     outportb(ti_port[active_port].modem_status,(unsigned char)5); /* drop modem*/
     outportb(ti_port[active_port].modem_status,(unsigned char)0); /* port DTR  */
     goto the_exit;
  }
  if(FromNet)
     goto the_exit;
  if(strlen(disable_string))
     TImoPuts(disable_string);
  else

the_exit:
     writeTIport(5,(unsigned char)0x68); /* write to UART register 5; drop DTR and RTS */
  delay(10);
}

/*********************************************************/
/*    EnableModem() raises DTR to enable autoanswer      */
/*********************************************************/

void EnableModem(char FromNet)  {
  if(internalmodem)
  {
     outportb(ti_port[active_port].modem_status,(unsigned char)5);
     outportb(ti_port[active_port].modem_status,(unsigned char)128);
     /* this raises modem port DTR */
  }
  if(FromNet)
    writeTIport(5,(unsigned char)0xEA); /* write to UART register 5; raise DTR and RTS */
  else
  {
    if(strlen(enable_string))
       TImoPuts(enable_string);
    else
       writeTIport(5,(unsigned char)0xEA); /* write to UART register 5; raise DTR and RTS */
  }
}

/*************************************************************************/
/*   setNetCallBaud sets the baud rate for networking and dialing out.   */
/*************************************************************************/

int setNetCallBaud(int targetBaudCode)
{
   if(TIlockport)  {
      setTIbaudrate(TIlockport);
      return(TRUE);
   }
   if(targetBaudCode <= sysbaud) {
      setTIbaudrate(targetBaudCode);
      return(TRUE);
   }
      setTIbaudrate(sysbaud);
      return(TRUE);
}

/************************************************************************/
/*      TImoPuts() Put a string out to modem without carr check         */
/************************************************************************/
void TImoPuts(char *s)  {
    while (*s) {
        delay(5);
        outMod(*s++);
    }
}

/************************************************************************/
/*     TIgetMod gets a char from the modem and strips the parity bit    */
/************************************************************************/

char TIgetMod(void)
{
        return Citinp() & 0x7F;
}

/*********************************************************************/
/*  RottenDial() is the internal modem dial function.  Since the     */
/*  internal modem is not Hayes compatible, we have to deal with     */
/*  it here.  You need the internal modem specs to understand this.  */
/*********************************************************************/

int RottenDial(char *callout_string)  {
int count,tries,result,ring_count = 1;

   if(!internalmodem)
      return FALSE;

   EnableModem(TRUE);
   setTIbaudrate(0);
   outportb(ti_port[active_port].modem_status,(unsigned char)5);
   outportb(ti_port[active_port].modem_status,(unsigned char)2);
   outportb(ti_port[active_port].uart_control,(unsigned char)0);

   while (MIReady()) Citinp();

   for(count=1;count<32000;count++)  {
      if((inportb(ti_port[active_port].uart_control) & 8) != 0)
        break;
   }
   outMod('W'); /* resets modem */
   for(count=1;count<32000;count++)  {
      if(MIReady)  {
         if(TIgetMod() == 'A')
            break;
      }
   }
   for(count = 1;count <= 30000;count++); /* delay */
   if(sysbaud >= 1) {
      outMod('H'); /* sets high speed */
         for(count=1;count<32000;count++)  {
            if(MIReady)  {
               if(TIgetMod() == 'A')
                  break;
            }
         }
   }
   while (MIReady()) Citinp();
   TImoPuts(callout_string);

   for(count=1;count<32000;count++)  {
      if(MIReady)  {
         if(TIgetMod() == 'A')
            break;
      }
   }
   outportb(ti_port[active_port].modem_status,(unsigned char)5);
   outportb(ti_port[active_port].modem_status,(unsigned char)128);

   for(tries = 0;tries <=4; tries++)
   {
      for(count = 1; count<32000; count++)  {
         if(MIReady)  {
            if(TIgetMod() == 'E')
               goto ring;
         }
      }
   } /* this whole constuct is a timeout loop with an exit */

goto xit;
ring:
   for(tries=1; tries<=8; tries++)
   {
      if(gotCarrier())
         break;
      for(count = 1; count<32000; count++)
      {
         if(MIReady())
         {
            result = TIgetMod();
            if((result == 'B') || (result == 'O'))
               {
                  vputch(result);
                  if(!gotCarrier())
                  goto xit;
               }
            if(result == 'R')
            {
               ring_count++;
               if(ring_count == 6)  /* let it ring 6 times */
                  goto xit;
               vputch(result);
               result=0;
               goto ring;
            }
         }
      }
   }
   if(!gotCarrier)
      goto xit;
   vputch(' ');
   outportb(ti_port[active_port].uart_control,(unsigned char)0);
   if(sysbaud >= 1)  {
      outportb(ti_port[active_port].uart_control,(unsigned char)14);
      outportb(ti_port[active_port].uart_control,(unsigned char)0);
      setTIbaudrate(1);
      outportb(ti_port[active_port].uart_control,(unsigned char)14);
      outportb(ti_port[active_port].uart_control,(unsigned char)227);
      outportb(ti_port[active_port].uart_control,(unsigned char)15);
      outportb(ti_port[active_port].uart_control,(unsigned char)0);
   }
   return TRUE;  /* we did the dialing */
xit:
   vputch(' ');
   outportb(ti_port[active_port].modem_status,(unsigned char)5); /* drop modem*/
   outportb(ti_port[active_port].modem_status,(unsigned char)0); /* port DTR  */
   writeTIport(5,(unsigned char)0x68); /* write to UART register 5; drop DTR and RTS */
   delay(10);
   return BUSY;
}

/* video functions */

/***************************************/
/*  StopVideo() erases the statusline  */
/***************************************/

void  StopVideo(void) {
   int k;
   directvideo=0;
   vatt = BLACK;
   vlocate ( 24, 0 );
   for ( k = 0; k < 80; k++ )
      vputch(' ');
   vatt = NORM;
   *attrlatch = vatt;
   vlocate ( 24, 0 );
}

/**************************************************************/
/* video() initializes the video and prints a statusline tag  */
/**************************************************************/

void video ( char *tagline )
{
   int k;

   directvideo=0;
   clrscr();

   NORM = ( (temp=getenv("CTEXT"))==0) ? 5 : atoi(temp);
   CONTRAST = ( (temp=getenv("CSTAT"))==0) ? 21 : atoi(temp);
/* get the color if set by envirnment variable, else use defaults colors */

   if(NORM>=17)     NORM=24+(NORM % 8);
   if(NORM==8)      NORM=15;                   /* all these if statements do  */
   if(NORM<8)       NORM+=8;                   /* is to change the color      */
   if(CONTRAST>=17) CONTRAST=24+(CONTRAST %8); /* into numbers the video card */
   if(CONTRAST==8)  CONTRAST=15;               /* can understand...           */
   if(CONTRAST<8)   CONTRAST+=8;

   if((NORM >=32) || (CONTRAST >=32))
   {
      vlocate(23,0);
      vputs("Error: color setting too large.  Using default color instead, check settings");
      NORM = (NORM >=32) ? 21 : NORM;
      CONTRAST = (CONTRAST >= 32) ? 5 : CONTRAST;
   }
   vatt=NORM;

   vlocate ( 24, 0 );
/* vatt = (InvVideo) ? CONTRAST : NORM;  */
   vatt = CONTRAST;
   for ( k = 0; k < 80; k++ )
      vputch ( ' ' );
   vlocate ( 24, 0 );
   vputs ( tagline );
   vatt = NORM;
   vlocate ( 23, 0 );
   vbot=23; vtop = 0;
}

/***************************************/
/*     vlocate() emulates gotoxy()     */
/***************************************/

void vlocate ( unsigned char row, unsigned char col )
{
   _BX = 0;
   _DL = row;
   _DH = col;
   _AX = _DX;
   _AH = 2;

   geninterrupt ( 0x49 );     /* TI CRT device service routine interrupt */
   vrow = row; vcol = col;
}

/**************************************************************/
/*      vputs() writes a string to the system console         */
/**************************************************************/

void vputs ( char *string )
{
/* char *old; old = string; */
   while ( *string )
      vputch ( *string++ );
/*   return ( old - string );  */
}

/**************************************************************/
/*   vbump() keeps the statusline from scrolling away         */
/**************************************************************/

void vbump ( void ) {
   if(vrow == 24)
     return;

   _AH = 6;         /* scroll */
   _CH = vtop;
   _DH = vbot;
   _CL = vleft;
   _DL = vright;
   _BH = 7; /*vatt;*/
   _AL = 1;         /* number of lines */
   geninterrupt ( 0x10 );
}


/**************************************************************/
/*   vputch() writes a char to the system console. Keeps      */
/*   track of the cursor position at all times.               */
/**************************************************************/

void vputch ( char c )
{
static int kount=0;

   if( c >= 32)  /* if c is a printable character */
   {
         *attrlatch = vatt;    /* set the char's color        */
         _AL = c;              /* character to write in AL    */
         _AH = 0x0E;           /* CRT DSR command in AH       */
         geninterrupt(0x49);   /* INT 49 calls TI Pro CRT DSR */
         *attrlatch = vatt;    /* set the char's color        */

         if ( vcol == vright ) {
            vcol = vleft;
            if ( vrow == 23 /*vbot*/ )
            {
               _AH = 6;         /* scroll */
               _CH = vtop;
               _DH = vbot;
               _CL = vleft;
               _DL = vright;
               _BH = 7; /*vatt;*/
               _AL = 1;         /* number of lines */
               geninterrupt ( 0x10 );
               goto locate;
            }
/*             vbump ( ); if statement above replaces this */
            else
               vrow++;
         }
         else
            vcol++;
   goto vxit;
   }
   if( c == '\r')
   {
      vcol = vleft;
      goto locate;
   }
   if( c == '\n')
   {
      if ( vrow == vbot )
      {
         _AH = 6;         /* scroll */
         _CH = vtop;
         _DH = vbot;
         _CL = vleft;
         _DL = vright;
         _BH = 7; /*vatt;*/
         _AL = 1;         /* number of lines */
         geninterrupt ( 0x10 );
      }
/*          vbump ( ); if statement above replaces this.... */
      else
         vrow++;
      goto locate;
   }
   if( c == '\b')
   {
      if ( vcol == vleft ) {
         if ( vrow != vtop ) {
            vlocate ( vrow - 1, vright );
            vputch ( ' ' );
            vlocate ( vrow - 1, vright );
         }
      }
      else {
         vlocate ( vrow, vcol - 1 );
         vputch ( ' ' );
         vlocate ( vrow, vcol - 1 );
      }
      goto locate;
   }
   if( c == '\t')
   {
      for ( kount = 0; kount < 8; kount++ )
            vputch ( ' ' );
      goto locate;
   }
   if( c == '\a')
   {
      _AH = 14;
      _AL = c;
      _BH = 0;
      geninterrupt ( 0x10 );
/*    goto locate; */
   }
locate:

   _BX = 0;
   _DL = vrow;
   _DH = vcol;
   _AX = _DX;
   _AH = 2;
   geninterrupt ( 0x49 );     /* TI CRT device service routine interrupt */
vxit:
   return;
}

/*****************************************************************/
/*   statusline() writes a statusline on line 25 of the console  */
/*****************************************************************/

void statusline ( char *string )
{
/* 21 is the column where the statusline begins... */

   int k;
   unsigned char ocol = vcol, orow = vrow;
   vlocate ( 24, 21 );
   vatt = CONTRAST;
   for ( k = 21; k < 80; k++ )
      vputch(' ');
   vlocate ( 24, 21 );
   vputs ( string );
   vatt = NORM;
   *attrlatch = NORM;
   vlocate ( orow, ocol );
}
