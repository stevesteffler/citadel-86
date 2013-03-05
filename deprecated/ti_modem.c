/*  Citadel-86/TI Professional Computer BBS modem functions...
 *
 *  (c) Copyright Robert Nelson (The Grey Knight), Saint Paul, MN. 1990,1991.
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
 *  This file, TI_MODEM.C, with TI_VIDEO.C, TI_PRO.H and TI_DELAY.LIB, contains
 *  all the required functions to make Citadel-86 run on the TI Professional
 *  Computer. ( Turbo C's delay() uses IBM hardware not present on the TIPC )
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*#include <conio.h>*/
#include <dos.h>

#include "ti_pro.h" /* the #defines and function prototypes are in this file */




/*                                 externs                                    */


extern int directvideo;  /* We'll set Turbo C's direct IBM screen writes off. */

extern int sysbaud = 0;  /* Max possible system baud rate, init to 300 baud.  */




/*                                 globals                                    */

void interrupt (*com_addr)();              /* old com vector addr storage */
static char intSet = FALSE;                /* has comint been revectored ? */

/*static unsigned char combuf[COMBUF_SIZE];*/  /* allocate the receive buffer */

static unsigned char receive_buffer[COMBUF_SIZE];
  /* allocate the receive buffer */
static unsigned char * combuf = receive_buffer;

static unsigned buf_size = (unsigned) COMBUF_SIZE;    /* rx buffer's size */
static unsigned buffer_count = (unsigned)COMBUF_SIZE; /* rx buffer free space */

static unsigned char old_inta01;           /* old vector mask storage */
static int cptr, head;                     /* point to positions in buffer */
static int buffer_full = 0;                /* flag indicates busy status */
static int buf_is_on = 0;                  /* is high speed buffering on? */

static unsigned char active_port;          /* stores the active port number */
static int TIlockport = 0;                 /* port locking flag */
static char internalmodem = 0;             /* internal modem flag */

static char ptr_insurance[] = "";              /* '\0' string to point to */
static char *init_string = ptr_insurance;      /* modem init string */
static char *high_speed_init = ptr_insurance;  /* high speed modem init */
static char *enable_string = ptr_insurance;    /* enable modem string */
static char *disable_string = ptr_insurance;   /* disable modem string */

typedef struct {                 /* structure for port variables */
      int  uart_control;         /* 8530 control port address */
      int  uart_data;            /* 8530 data port address */
      int  modem_status;         /* 8530 modem status register address */
      int  com_vector;           /* 8530 interrupt level number storage */
      unsigned char PIC_mask;    /* interrupt mask storage */
} UARTDATA;

UARTDATA ti_port[4] = {               /* data for: */
        {0xe6,0xe7,0xe4,0x40,0xfe},   /* port 1 */
        {0xee,0xef,0xec,0x41,0xfd},   /* port 2 */
        {0xf6,0xf7,0xf4,0x42,0xfb},   /* port 3 */
        {0xfe,0xff,0xfc,0x44,0xef} }; /* port 4 */
/* uart  ctrl,data,stat,vctr,mask
 *
 * Vector 43 is not a com vector! The 40,41,42,44 sequence IS CORRECT!
 *
 */

/*                        end of global variables                             */





/*                            Modem functions                                 */

/******************************************************************************/
/* TiModemOpen() initializes and enables the 8530 UART and initializes the    */
/* global variables with values passes from Citadel and CTDLCNFG/CTDLTABL.SYS */
/******************************************************************************/

void TiModemOpen(char FromDoor, char internal, int portnumber,
                  int sysbaudrate, char *ModemInitString, int LockPortAtBaud,
                  char *HiSpeedInitString, char *EnableString,
                  char *DisableString)
{
  unsigned char bite;  /* work area for calculating the interrupt mask */

  directvideo = 0;  /* make sure that's shut off, TURBO C's global variable */
                    /* in case a later version of Cit-86 should set it on.  */

/**************************************/
/*  This entire section loads global  */
/*  variables with the passed values  */
/*                                    */

  if(strlen(ModemInitString))
     init_string = ModemInitString;
  if(strlen(HiSpeedInitString))
     high_speed_init = HiSpeedInitString;
  if( (strlen(EnableString)) && (strlen(DisableString)) )
  {
     enable_string = EnableString;
     disable_string = DisableString;
  }
  /* see ptr_insurance in the global declaration section above if these
     aren't obvious... */

  sysbaud = sysbaudrate;

  if(internal) {           /* the internal modem must be either 300 or 1200. */
     internalmodem = internal;
     sysbaud = (sysbaudrate == 1) ? 1 : 0;   /* 0 = 300, 1 = 1200 */
  }

  active_port = portnumber;  /* which port to use */

  TIlockport = (internalmodem) ? 0 : LockPortAtBaud;
  /* internal modem does not support port locking */

  cptr = head = 0;  /* circular receive queue initialize */

/* values have been loaded... */
/******************************/
/* now we set up the UART...  */

  old_inta01 = bite = inportb(TI_INTA01); /* save the old interrupt */

/**********************************/
/* start of if(!FromDoor) test... */

/* if Citadel is being brought up as a door itself, we cannot disturb the
   port settings.  Do a complete reset and set things up if not... */

  if(!FromDoor) {

    writeTIport( 9,(unsigned char)0xC0);
    /* 11000000  master hardware reset */

    writeTIport( 9,(unsigned char)0x40);
    /* 01000000  reset channel B */

    writeTIport( 4,(unsigned char)0x4E);
    /* 01001110  2 stop, no parity, baud rate gen == times 16 */

    writeTIport( 2,(unsigned char)TI_INTA01);
    /* tell the UART the TI's 8259 interrupt controller address */

    writeTIport( 3,(unsigned char)0xC0);
    /* 11000000  receive will be 8 bits, no auto enable */

    writeTIport( 5,(unsigned char)0x60);
    /* 01100000  transmit  will be 8 bits */

    writeTIport( 6,(unsigned char)0x00);
    /* 00000000  enable asynchronous mode */

    writeTIport( 7,(unsigned char)0x00);
    /* 00000000  enable asynch mode */

    writeTIport( 9,(unsigned char)0x01);
    /* 00000001  vector includes status */

    writeTIport(10,(unsigned char)0x00);
    /* 00000000  enable asynch mode */

    writeTIport(11,(unsigned char)0x56);
    /* 01010110  baud rate generator initialize */

    writeTIport(12,(unsigned char)0xFF);
    /* 11111111  low byte of baud constant for 300 baud */

    writeTIport(13,(unsigned char)0x00);
    /* 00000000  high byte of baud constant for 300 baud */

    if(internalmodem)         /* cannot do port locking on the internal modem */
       setTIbaudrate(sysbaud);
    else
       setTIbaudrate((sysbaud > TIlockport) ? sysbaud : TIlockport);
       /* else set to highest speed supported */

    writeTIport(14,(unsigned char)227);
    /* 11100011  enable baud rate generator, with source SYSCLOCK */
    /* system clock.  Also sets NRZI mode */

  } /* end of if !FromDoor test */
/********************************/

/* we set all the following 8530 and 8259 values whether Citadel is being
   brought up as a door or not; all operations that follow are non-
   destructive to the state of the UART or the active port. */

    writeTIport(15,(unsigned char)0x00);
   /* 00000000  turn off all status interrupts */

/*  enable interrupts */

    writeTIport( 9,(unsigned char)0x09);
    /* 00001001  master interrupt enable, vector includes status */

    writeTIport( 1,(unsigned char)0x10);
    /* 00010000  generate an interrupt on a received character enable */

/* vector the interrupt */

    bite &= ti_port[active_port].PIC_mask;
    /* AND the old mask with the port mask to make the new interrupt mask */

    outportb(TI_INTA01, bite);              /* and set it */
    /* this preserves the 8259's active vector table, and adds our vector */

    com_addr = getvect(ti_port[active_port].com_vector);  /* save old vector */
    setvect(ti_port[active_port].com_vector, comint);     /* set the new one */

    intSet = TRUE ; /* keep track of whether the com interrupt is revectored */

/* raise DTR, CTS, and RTS */

/*  set interrupts */

    writeTIport( 3,(unsigned char)0xC1);
    /* 11000001  enable receive, 8 data bits */

    writeTIport( 5,(unsigned char)0xEA);
    /* 11101010  enable transmit, raise DTR, RTS, 8 data bits */


    if(!FromDoor)
       Reinitialize();
    /* if brought up as a door, we don't want to reinitialize the modem... */

}

/******************************************************/
/*      comint() Handles the TI Pro interrupt         */
/******************************************************/

void interrupt comint()
{
    unsigned char cbuf;    /* temporary received char storage */
    unsigned int lsr_data; /* storeage for UART status data */

/* you definitely need the spec sheet on the Intel 8530 UART to be able to
   understand this... */

        lsr_data = inportb(ti_port[active_port].uart_control);
        /* find out what caused the interrupt */

        if ((lsr_data & (unsigned int)0x01) == 0) {
            outportb(ti_port[active_port].uart_control, (unsigned char)0xea);
            outportb((int)TI_INTA00, (unsigned char)EOI);
            outportb(ti_port[active_port].uart_control,(unsigned char)0x20);
            return;
        } /* if something else caused the interrupt besides a received
             character, just return.  Otherwise, common framing errors will
             interrupt a protocol file transmission; better to put up with
             the line noise... transmit buffer empty interrupt is not
             implemented. */

        /* else we have a new character to store in the buffer, so store it */

        cbuf = inportb(ti_port[active_port].uart_data);
        if (cptr >= buf_size)
            cptr = 0;
        /* if at the end of the buffer, start again at the beginning */

        combuf[cptr++] = cbuf; /* add the recvd char to the buffer */

        --buffer_count; /* shows decrease in distance between cptr and head */

        if(buffer_count < 128) {    /* the buffer is about to be overrun */
           buffer_full = TRUE;      /* not 1; leave 128 for modemPushBack() */
           writeTIport( 5,(unsigned char)0xE8);
          /* 11101000  keeps DTR, 8 bits, and tx enable, lowers RTS */
        } /* even if RTS is low, a char could still be on it's way, so: */

        outportb(ti_port[active_port].uart_control,(unsigned char)0x20);
        /* enable receive */

        outportb(ti_port[active_port].uart_control, (unsigned char)0xea);
        /* Tell the 8530 we acknowledge the interrupt */

        outportb((int)TI_INTA00, (unsigned char)EOI);
        /* send end of interrupt to the 8259 */

        return;
}

/************************************************************************/
/* ModemPushBack puts a char back into the input buffer, like ungetch() */
/************************************************************************/

void ModemPushBack(char c)
{

     if(buffer_full)      /* temporary kludge: talk to Hue, Jr, and ask */
        if(!buffer_count) /* him to modify this to return an error value */
           return;        /* in case of a buffer overrun */

     --buffer_count;      /* update cptr/head distance counter */

     if(head > 0)         /* head points to our current buffer read position */
        --head;

     else                 /* if we're at the beginning of the buffer, then
                             point to the physical end of the buffer */
        head = combuf[buf_size-1];

     combuf[head] = c;  /* probably redundant, since head now points to the */
                        /* the last character we supposedly removed...      */
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

      /* install the UART register reset and reload here */

      outportb(TI_INTA01, old_inta01);
      setvect(ti_port[active_port].com_vector, com_addr);
      /* These reinstall the old interrupt vector */

      intSet=0; /* Citadel no longer controls the vector, so set this */
   }
   else
      if(KillCarr)
         DisableModem(TRUE);
}


/*********************************************/
/* transmit buffering is not yet implemented */
/*********************************************/

void BufferingOn(void) {

unsigned i;
unsigned char * temp = NULL;

   for(i = (unsigned)65520L;i >= (unsigned)COMBUF_SIZE; i -= (unsigned)256)  {
      if( (temp = (unsigned char *) calloc(i,sizeof(unsigned char *)) ) != NULL)
         break;  /* try to allocate as much memory as possible, starting with */
   }             /* just under 64K. If unsuccessful, try a smaller amount */

   if( i <= (unsigned) COMBUF_SIZE) {
      if(temp != NULL)
         free((unsigned char *)temp);
      return; /* may as well not even turn buffering on if this is true */
   }
   else {
      combuf = temp;     /* else reassign the rx buffer pointers to the new */
      cptr = head = 0;   /* larger rx buffer, and set the buf_is_on flag.   */
      buf_size = i;
      buffer_count = i;
      buf_is_on = 1;     /* set flag for BufferingOff() */
   }
   return;
}

void BufferingOff(void) {

   if(!buf_is_on)
      return;
   buf_is_on = 0;

   free((unsigned char *)combuf);          /* see BufferingOn() */
   combuf = receive_buffer;
   cptr = head = 0;
   buf_size = (unsigned) COMBUF_SIZE;
   buffer_count = (unsigned) COMBUF_SIZE;
   return;

}


/************************************************************************/
/*  gotCarrier() returns TRUE if there is carrier at the modem port.    */
/************************************************************************/

int gotCarrier(void) {
   return( (inportb(ti_port[active_port].uart_control) & 0x08) ? 1 : 0 );
}

/************************************************************************/
/*     writeTIport() writes an internal 8530 UART control register      */
/************************************************************************/

void writeTIport(int rgster,unsigned char value)
{
    /* if register 0, write, else write register 0 to select the internal 8530
       register the write will address */

    if(rgster != 0) outportb(ti_port[active_port].uart_control,
                               (unsigned char)(rgster & 0x0F));
                               /* leave the upper 4 bits alone...  */
    outportb(ti_port[active_port].uart_control,(unsigned char)value);

/* to write an 8530 internal register, we write the register number to the
 * control port, which is register 0.  This makes the 8530 store the next
 * write to register 0 in the specified internal 8530 register.
 */

}

/************************************************************************/
/*           setTIbaudrate() sets the UART baud rate                    */
/************************************************************************/

void setTIbaudrate(int speed) {
      switch(speed) {
           case  0:  writeTIport(12,(unsigned char)0xFF);
                     break;    /* set 300 baud */
           case  1:  writeTIport(12,(unsigned char)0x3F);
                     break;    /* set 1200 baud */
           case  2:  writeTIport(12,(unsigned char)0x1F);
                     break;    /* set 2400 baud */
           case  3:  writeTIport(12,(unsigned char)0x0E);
                     break;    /* set 4800 baud */
           case  4:  writeTIport(12,(unsigned char)0x06);
                     break;    /* set 9600 baud */
           case  5:  writeTIport(12,(unsigned char)0x03);
                     break;    /* set 14400 baud */
           case  6:  writeTIport(12,(unsigned char)0x02);
                     break;    /* set 19200 baud */
           default:  vputs(" ERROR: check CTDLCNFG baud rate setting.\n");
                     exit(CRASH_EXIT); /* return an ERRORLEVEL of 2 to DOS  */
                                       /* so batch file knows CTDL crashed. */
                                       /* other exit numbers are reserved.  */
      }
           return;
}


/************************************************************************/
/*      outMod and fastMod stuff a char out the modem port              */
/*      fastMod is #defined in ti_pro.h as outMod for compatibility     */
/*      with the older version Citadel source.                          */
/************************************************************************/
char outMod(int c)
{
   if(!(inportb(ti_port[active_port].uart_control) & 0x08 ))
      do { /* if no carrier, assume we're sending a string to the modem */
         outportb(ti_port[active_port].uart_control, (unsigned char)0x05);
      } while ((inportb(ti_port[active_port].uart_control) & 0x24 ) != 0x24);
        /* read register five, wait for condition CTS high (0x20) and */
        /* tx buffer empty (0x04) so we can send, or loop. */
   else{
      do {
           if(!(inportb(ti_port[active_port].uart_control) & 0x08 )) {
              delay(10); /* Carrier loss ?  Wait and recheck to be sure */
              if(!(inportb(ti_port[active_port].uart_control) & 0x08 ))
                 return(0);  /* if carrier loss, return FALSE */
           }
           outportb(ti_port[active_port].uart_control, (unsigned char)0x05);
           /* select 8530 internal register register 5 to read from */

      } while ((inportb(ti_port[active_port].uart_control) & 0x24 ) != 0x24);
        /* read register five, wait for condition CTS high (0x20) and */
        /* tx buffer empty (0x04) so we can send, or loop. */
   }
   outportb(ti_port[active_port].uart_data, (unsigned char)c);
   return TRUE;
}

char fastMod(int c)
{
    outMod(c);
    return TRUE;

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
/*      MIReady() Ostensibly checks to see if input from modem ready    */
/************************************************************************/
int MIReady(void)
{
    return (cptr != head);
   /* cptr++ when a char is received, head++ when a char is retrieved */
}


/************************************************************************/
/*      Citinp() reads data from port.  Should not be called if there is   */
/*      no data present (for good reason).                              */
/************************************************************************/

unsigned char Citinp(void) {

   ++buffer_count; /* the distance between cptr and head increases */

   if(head >= buf_size)     /* if we're at the physical end of the circular */
      head = 0;             /* buffer, reposition to the beginning.         */

   if(buffer_full) {
      if(buffer_count >= (unsigned)256) {  /* 256 ( and 128 ) are arbitrary */
         buffer_full = FALSE;
         writeTIport( 5,(unsigned char)0xEA);
         /* 11101010  raises RTS, keeps tx enable, DTR, 8 data bits */
      }
   }
   return(combuf[head++]);
}


/************************************************************************/
/* HangUp() hangs the modem up and then re-enables it.  The parameter   */
/* lets you do a final pause, if necessary, on user log outs (otherwise,*/
/* if you do port locking, LONOTICE can be partially lost).             */
/************************************************************************/

void HangUp(char FromNet)  {
    if(!FromNet)
      delay(100); /* make sure all characters are sent before dropping DTR */

    writeTIport(5,(unsigned char)0x68);
    /* write to UART register 5; drop DTR and RTS */

    delay(50); /* for slow modems */

    ReInitModem(); /* set system baud rate to highest supported */

    writeTIport(5,(unsigned char)0xEA);
    /* write to UART register 5; raise DTR and RTS, tx enable, 8 data bits */

    if(strlen(high_speed_init))
       TImoPuts(high_speed_init);
    else
       TImoPuts(init_string);
    /* sends the correct init string to modem */
}

/*************************************************************/
/*    Reinitialize() reinitializes the port and the modem    */
/*************************************************************/

void Reinitialize(void) {

   cptr = head = 0;
   buffer_count = buf_size;
   writeTIport(5,(unsigned char)0xEA);
   /* write to UART register 5; raise DTR and RTS, tx enable, 8 data bits */
   TImoPuts(init_string); /* sends init string to modem */
}

/*****************************************************************/
/*    ReInitModem() sets port to the highest system baud rate    */
/*****************************************************************/

void ReInitModem(void)  {

   cptr = head = 0;
   buffer_count = buf_size;

   if(internalmodem)
      setTIbaudrate(sysbaud); /* internal modem does not support port locking */
   else
      setTIbaudrate((sysbaud > TIlockport) ? sysbaud : TIlockport);
   /* if port locking is being done, set to the CTDLCNFG.SYS LOCK-PORT speed */
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
     outportb(ti_port[active_port].modem_status,(unsigned char)5);
     outportb(ti_port[active_port].modem_status,(unsigned char)0);
     /* drop internal modem's DTR */
     goto the_exit;
  }
  if(FromNet)
     goto the_exit;
  /* if not a network session, send the disable modem string, if present. */
  if(strlen(disable_string))
     TImoPuts(disable_string);
  else

the_exit:
     writeTIport(5,(unsigned char)0x68);
     /* write to UART register 5; drop DTR and RTS */
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
      /* this raises the internal modem's DTR */
   }
   if(FromNet)
      /* don't initialize the modem if we are in a network session */
      writeTIport(5,(unsigned char)0xEA);
      /* write to UART register 5; raise DTR and RTS */
   else
   {
     if(strlen(enable_string))
        TImoPuts(enable_string);
     /* if there exists a modem init string, send it to the modem */
     else
        writeTIport(5,(unsigned char)0xEA);
        /* write to UART register 5; raise DTR and RTS, tx enable, 8 bits */
   }
   cptr = head = 0;
   buffer_count = buf_size;
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
   /* if port locking is used, set to the locked speed */

   /* targetBaudCode is the highest speed the system we are calling is
      capable of supporting.  If we can't handle that speed, set to the
      highest speed we do support, else set to their max speed */

   if(targetBaudCode <= sysbaud)
      setTIbaudrate(targetBaudCode);
   else
      setTIbaudrate(sysbaud);
   return(TRUE);
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
/*                                                                   */
/*  This function is well-named, so be prepared.  TI thought they    */
/*  had better ideas, so we must endure the pain that follows below. */
/*********************************************************************/

int RottenDial(char *callout_string)  {
int count,tries,result,ring_count = 1;

   if(!internalmodem)
      return FALSE;  /* an external modem is being used, so let CTDL
                        know to use it's own dialout function.    */

   EnableModem(TRUE);

   setTIbaudrate(0);  /* set the baud rate to 300 */

   outportb(ti_port[active_port].modem_status,(unsigned char)5);
   outportb(ti_port[active_port].modem_status,(unsigned char)2);
   outportb(ti_port[active_port].uart_control,(unsigned char)0);
   /* tell the modem we want it to dial out, this raises RTS */

   while (MIReady()) Citinp();  /* clear any garbage out of the rx buffer */

   for(count=1;count<32000;count++)  {
      if((inportb(ti_port[active_port].uart_control) & 8) != 0)
        break;
   }
   /* this makes sure the command register has time to link to the
      modem registers correctly */

   outMod('W'); /* resets modem */
   for(count=1;count<32000;count++)  {
      if(MIReady)  {
         if(TIgetMod() == 'A')
            break;
      }
   }
   /* wait for the acknowledgement, assume we missed it if loop times out */

   for(count = 1;count <= 30000;count++); /* delay */
   if(sysbaud >= 1) {
      outMod('H'); /* tells the modem to use 1200 baud */
         for(count=1;count<32000;count++)  {
            if(MIReady)  {
               if(TIgetMod() == 'A')
                  break;
            }
         }
   }
   /* wait for the acknowledgement, assume we missed it if loop times out */

   while (MIReady()) Citinp();  /* clear garbage out of the rx buffer */
   TImoPuts(callout_string); /* and tell the modem the number to dial */

   for(count=1;count<32000;count++)  {
      if(MIReady)  {
         if(TIgetMod() == 'A')
            break;
      }
   }
   /* wait for the acknowledgement, assume we missed it if loop times out */

   outportb(ti_port[active_port].modem_status,(unsigned char)5);
   outportb(ti_port[active_port].modem_status,(unsigned char)128);
   /* directly write the UART's channel B registers ( external modems
      require only the A register, and raise the modem's DTR. */

   /* While we wait in the loop that follows, the modem dials. */

   for(tries = 0;tries <=4; tries++)
   {
      for(count = 1; count<32000; count++)  {
         if(MIReady)  {
            if(TIgetMod() == 'E')
               goto ring;
         }
      }
   } /* this whole constuct is a timeout loop with an exit.  The modem
        returns 'E' after it dials.  This timeout loop has been tested
        and is correct.  If an 'E' is not returned by the end, a problem
        has been encountered, so be bail out to the exit below.  One of
        the few constructs where the use of a goto is indicated...  */

goto xit; /* the call failed: bail out and return BUSY to indicate failure */

ring:   /* Another good use of a goto.  Too many loops as it is, below,
           and we must be prepared to break out of all of them at once.
           The phone will ring until the modem times out, or the phone rings 7
           times.  As long as it rings, we return to here.  If we get a busy
           or a connection, we jump out.
           20 percent of the time or so, the call progress detection fails,
           and the modem can't tell if we have a ring or a busy signal.
           So, we have a timeout loop, so we can bail out if this happens.

           tries<=8 is used as an arbitrary value.  It creates a loop that
           lasts longer than the modem's internal timeout. */

   for(tries=1; tries<=8; tries++)
   {
      if(gotCarrier())  /* jump out if we have carrier */
         break;

      for(count = 1; count<32000; count++)  /* timeout loop */
      {
         if(MIReady())  /* the modem has returned a status character */
         {
            result = TIgetMod();
            if((result == 'B') || (result == 'O')) /* busy or error condition */
               {
                  vputch(result);    /* display the result (B) on screen */
                  if(!gotCarrier())
                  goto xit;          /* and bail out, return BUSY status */
               }
            if(result == 'R')
            {
               ring_count++;
               if(ring_count == 7)  /* let it ring 7 times */
                  goto xit;         /* no one's home... bail out */

               vputch(result);      /* display the result (R) on screen */

               result=0;            /* check this: probably redundant */

               goto ring;           /* and wait for an answer or timeout */
            }
         }
      }
   }
   if(!gotCarrier)
      goto xit;   /* if no carrier, we bail out */

   /* otherwise, we have carrier, and need to adjust UART channel A */
   vputch(' '); /* cosmetic */

   outportb(ti_port[active_port].uart_control,(unsigned char)0);
   /* select register 0, probably redundant, but safe */

   if(sysbaud >= 1)  {

      writeTIport(14,(unsigned char)0);
      /* 00000000  disable baud rate generator */

      setTIbaudrate(1); /* set 1200 baud */

      writeTIport(14,(unsigned char)227);
      /* 11100011  enable baud rate generator, with source SYSCLOCK */
      /* system clock.  Also sets NRZI mode */

      writeTIport(15,(unsigned char)0x00);
      /* 00000000  turn off all status interrupts */

   }
   return TRUE;  /* we did the dialing, and have carrier.  Return to caller. */

/* xit: is the bail out point.  The connection failed, so we reset the port and
   the modem, and return BUSY to indicate failure */

xit:
   vputch(' '); /* cosmetic */

   outportb(ti_port[active_port].modem_status,(unsigned char)5);
   outportb(ti_port[active_port].modem_status,(unsigned char)0);
   /* drop modem port DTR */

   writeTIport(5,(unsigned char)0x68);
   /* write to UART register 5; drop DTR and RTS */

   delay(10);   /* wait for modem to reset in case someone calls */
   return BUSY;
}

