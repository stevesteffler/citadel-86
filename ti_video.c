/*  Citadel-86/TI Professional Computer BBS video functions...
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
 *  This file, TI_VIDEO.C, with TI_MODEM.C, TI_PRO.H and TI_DELAY.LIB, contains
 *  all the required functions to make Citadel-86 run on the TI Professional
 *  Computer. ( Turbo C's delay() uses IBM hardware not present on the TIPC )
 */

#include <stdio.h>
#include <stdlib.h>
/*#include <string.h>*/
#include <conio.h>
#include <dos.h>

#include "ti_pro.h" /* the #defines and function prototypes are in this file */




/*                                 externs                                    */

extern int sysbaud;      /* the max possible system baud rate from TI_MODEM.C */

extern int directvideo;  /* We'll set Turbo C's direct IBM screen writes off. */





/*                                 globals                                    */

static char huge *attrlatch=(char huge *)0xDF800000l;

/* TI video card attribute latch, we write the attribute value to this latch,
 * and the character we wrote, and all that follow, will possess this attribute.
 */

static int norm;       /* color attribute to use for normal text display     */
static int contrast;   /* color attribute to use for statusline text display */

static unsigned char vatt = 0,  /* current color attribute (set BLACK here)  */
           vrow           = 0,  /* current row */
           vcol           = 0,  /* current column */
           vtop           = 0,  /* top line of screen */
           vbot           = 23, /* bottom line of the text display part of */
                                /* the screen.  Line 24 is the statusline  */
           vleft          = 0,  /* left column of screen */
           vright         = 79; /* right column of screen */

/* Set defaults.  Note the commas... */

/*                        end of global variables                             */





/*                            Video functions                                 */

/***************************************/
/*  StopVideo() erases the statusline  */
/***************************************/

void  StopVideo(void) {
   int k;
   vatt = BLACK;
   vlocate ( 24, 0 );
   for ( k = 0; k < 80; k++ )
      vputch(' ');
   vatt = norm;
   *attrlatch = vatt; /* write the attribute directly to the video card */
   vlocate ( 24, 0 );
}

/**************************************************************/
/* video() initializes the video and prints a statusline tag  */
/**************************************************************/

void video ( char *tagline )
{
   int count;
   char *temp;

   directvideo=0; /* Make sure Turbo C does not compile IBM direct screen
                     writes into the program. */

   clrscr();

   norm = ( (temp=getenv("CTEXT"))==0) ? 5 : atoi(temp);
   contrast = ( (temp=getenv("CSTAT"))==0) ? 21 : atoi(temp);
/* set screen colors if set by environment variables, else use default colors */

   if(norm>=17)     norm=24+(norm % 8);
   if(norm==8)      norm=15;                   /* all these if statements do  */
   if(norm<8)       norm+=8;                   /* is to change the color      */
   if(contrast>=17) contrast=24+(contrast %8); /* into numbers the TI Pro     */
   if(contrast==8)  contrast=15;               /* video card can understand...*/
   if(contrast<8)   contrast+=8;

   if((norm >=32) || (contrast >=32))
   {
      vlocate(23,0);
      vputs("Error: color setting too large.  Using default color instead, check settings");
      norm = (norm >=32) ? 21 : norm;
      contrast = (contrast >= 32) ? 5 : contrast;
   }

   vlocate ( 24, 0 );
   vatt = contrast;    /* set the current attribute to statusline text */
   for ( count = 0; count < 80; count++ )
      vputch ( ' ' );
   vlocate ( 24, 0 );  /* clear the statusline and reposition at column one */


   vputs ( tagline );  /* print the statusline */
   vatt = norm;        /* set color to normal text */
   vlocate ( 23, 0 );  /* and locate the line above the statusline. */
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

   geninterrupt ( 0x49 );   /* TI CRT device service routine interrupt */
   vrow = row; vcol = col;  /* update the cursor position variables    */
}

/**************************************************************/
/*      vputs() writes a string to the system console         */
/**************************************************************/

void vputs ( char *string )
{
   while ( *string )
      vputch ( *string++ );
}


/**************************************************************/
/*   vputch() writes a char to the system console. Keeps      */
/*   track of the cursor position at all times.               */
/**************************************************************/

/*  vputch() definitely needs to be improved.  When I wrote this,
 *  I did not have the technical reference manual for the video card,
 *  which I need to find out how to address the video card CPU directly.
 *
 *  I found that using IBM BIOS INT 10H calls, and having IBMULATE.COM
 *  handle them, was much faster, strangely enough, than using direct
 *  TI BIOS commands.  This is because IBMULATE bypasses the TI BIOS
 *  and writes directly to the video card's CPU.
 *
 *  The standard TI Pro BIOS scroll command is _very slow.  To see the
 *  scroll rate of the TI BIOS, run CTDL under EMULATE.COM rather than
 *  IBMULATE.COM.  It looks as though the screen is scrolling through
 *  molasses...  I was unable to use DEBUG to follow an IBM BIOS scroll
 *  command through IBMULATE, since it is impossible to trace through
 *  ROM, and IBMULATE uses a ROM call before it translates.
 *
 *  If anyone has the information needed to address the video card CPU
 *  directly, as IBMULATE does, or can find the author of IBMULATE, please
 *  modify this.  This would produce a considerable throughput improvement
 *  for callers reading text into their capture buffer at 4800/9600/19200
 *  baud, and eliminate the need to run Citadel under IBMULATE.
 *
 *  Use your editor and search for "IBM BIOS"...
 */

void vputch ( char c )
{
static int count=0;

/* The desired screen attribute must be set before calling vputch().
 * Uses cursor / attribute globals vatt, vrow, vcol, and screen definition
 * globals vtop, vbot, vleft, vright.  vatt is the current attribute,
 * vrow and vcol the current cursor position.  The others define the
 * four corners of the screen excluding the statusline.
 */

   /* see the beginning of the if c is a printable character statement ? */
   if( c >= 32)
   {
      *attrlatch = vatt;    /* set the char's color        */
      _AL = c;              /* character to write in AL    */
      _AH = 0x0E;           /* CRT Device Service Routine command */
      geninterrupt(0x49);   /* INT 49 calls TI Pro CRT DSR */
      *attrlatch = vatt;    /* set the char's color by writing the attribute
                               directly to the video card attribute latch */

      /* we wrote the character, now we move the cursor */
      if ( vcol == vright ) {  /* << see the bracket ? */

         vcol = vleft;     /* move from column 79 to column 0 */
         if ( vrow == 23 ) /* are we on the last line ? */
         {
            /* keep the statusline from scolling away */
            _AH = 6;         /* IBM BIOS scroll command. */
            _CH = vtop;      /* 0  */
            _DH = vbot;      /* 23 */
            _CL = vleft;     /* 0  */
            _DL = vright;    /* 79 */
            _BH = 7;      /* needs work. put the desired attribute here */
            _AL = 1;      /* number of lines to scroll */
            geninterrupt ( 0x10 ); /* IBM BIOS video interrupt 10H */
            goto locate;
         }
         else
            vrow++; /* we either scroll or drop down one row */
      } /* << the other bracket */
      else
         vcol++; /* otherwise just increment the cursor one to the right */
      return;
   } /* see the end of the if c is a printable character statement ? */

   if( c == '\r')    /* carriage return */
   {
      vcol = vleft;
      goto locate;   /* use goto for speed: no function call overhead */
                     /* could have used a switch, but hard to follow  */
   }
   if( c == '\n')    /* line feed */
   {
      if ( vrow == vbot ) /* are we on the last line ? */
      {
         /* keep the statusline from scolling away */
         _AH = 6;         /* IBM BIOS scroll command. */
         _CH = vtop;      /* 0  */
         _DH = vbot;      /* 23 */
         _CL = vleft;     /* 0  */
         _DL = vright;    /* 79 */
         _BH = 7;      /* needs work. put the desired attribute here */
         _AL = 1;      /* number of lines to scroll */
         geninterrupt ( 0x10 ); /* IBM BIOS video interrupt 10H */
      }
      else
         vrow++;   /* move down a line */
      goto locate;
   }
   if( c == '\b')  /* backspace */
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
   if( c == '\t') /* tab... recursive */
   {
      for ( count = 0; count < 8; count++ )
         vputch(' ');
      goto locate;
   }
   if( c == '\a') /* rare case */
   {
      _AH = 14;
      _AL = c;
      _BH = 0;
      geninterrupt ( 0x10 ); /* IBM BIOS video INT 10H */
   }
locate:  /* same as vlocate() function, copied here for speed */

   _BX = 0;
   _DL = vrow;
   _DH = vcol;
   _AX = _DX;
   _AH = 2;
   geninterrupt ( 0x49 );  /* TI CRT device service routine interrupt */
   return;
}

/*****************************************************************/
/*   statusline() writes a statusline on line 25 of the console  */
/*****************************************************************/

void statusline ( char *string )
{
/* 21 is the column where the statusline text begins... */

   int count;
   unsigned char ocol = vcol, orow = vrow; /* save the cursor position */

   vlocate ( 24, 21 ); /* take position on the statusline */
   vatt = contrast;    /* switch to statusline attribute */
   for ( count = 21; count < 80; count++ )
      vputch(' ');      /* clear the old statusline */
   vlocate ( 24, 21 );
   vputs ( string );    /* print the new one */
   vatt = norm;       /* switch back to the text diplay attribute */
   *attrlatch = norm; /* write the attribute directly to the attr latch */
   vlocate ( orow, ocol ); /* and restore the cursor position */
}


/************************************************************************/
/* changeBauds() queries the system operator for the new baud rate to   */
/* set the modem to, and then does so.                                  */
/************************************************************************/
int changeBauds(void)  {

unsigned char ocol = vcol, orow = vrow;
/* vcol and vrow are the current cursor position. ocol and orow are temporary
   storage for these old positions, so they can be restored upon returning  */

char msg[]=" Space cycles, return selects. Baud rate: ";
char *rates[] = {
        "300", "1200", "2400", "4800", "9600", "14400", "19200", "abort"
} ;

register int select = 0;  /* 0 = 300 baud */

/* Below, we use the space bar to cycle through the available baud rates.
   Maximum baud rate is contained in the variable sysbaud.  The variable
   select above contains the currently selected new baud rate value. */

      statusline(msg);  /* display msg[] on the statusline */
      vatt = contrast;  /* set the attribute to that of the statusline */

      while(TRUE) {
         vlocate(24,62);
         vputs("     ");        /* Take position on the stausline, erase the */
         vlocate(24,62);        /* previously displayed baud rate value, and */
         vputs(rates[select]);  /* display the next available baud value.    */

         while(!kbhit());       /* wait for input */

         if(getch() == '\r')    /* if enter is pressed, then select contains */
           break;               /* the value to set the baud rate to.        */

         select++;              /* otherwise increment select, and see if it */
         if(select == 8)        /* is still within the range of allowable    */
           select = 0;          /* values. If not, set it back to 0 and loop.*/
         if(select > sysbaud)
           select = 7;          /* rates[7] = "abort", allows user to abort  */
      }
      vatt = norm;
      vlocate ( orow, ocol );   /* take old cursor position and old attribute */
      if(select == 7)
         return(TRUE);
      else {
         setTIbaudrate(select);
         return(TRUE);
      }
}

