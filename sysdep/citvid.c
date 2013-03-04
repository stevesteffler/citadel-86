/* Citadel-86 BBS video routines...
 * (c) Copyright Pete Gontier (NO CARRIER), Santa Barbara, 1988.
 * These routines can be called 'freeware' in the sense that any use at all
 * is encouraged. The one exclusion is CompuServe - they will only
 * end up suing someone for downloading this and putting it on his BBS.
 * CompuServe may never have any claim to this file whatsoever.
 *
 * Modified 88Dec20 & 89Jan15 to handle color by HAW.
 * Modified 89Jan03 to handle funkified bell by HAW.
 */

#define VMODULE
#include "ctdl.h"
#include <dos.h>

#include "citvid.h"

ScreenMap *ScrColors;

extern SListBase BellList;

void video ( char *tagline ) {
   int k;
   vtop = 0;
   vatt = (ScrColors->ScrBack << 4) + ScrColors->ScrFore;
   vsetup ();
   vlocate ( 1, 0 );

   vatt = (ScrColors->StatBack << 4) + ScrColors->StatFore;
   for ( k = 0; k < 80; k++ )
      vputch ( ' ' );
   vlocate ( 1, 0 );
   vputs ( tagline );
   vlocate ( 24, 0 );
   vatt = (ScrColors->ScrBack << 4) + ScrColors->ScrFore;
   vtop = 2;
}

void vsetmode ( byte mode ) {
   _AH = 0;
   _AL = mode;
   geninterrupt ( VIDEO );
}

byte vgetmode ( void ) {
   _AH = 15;
   geninterrupt ( VIDEO );
   return ( _AL );
}

void vsetpage ( byte page ) {
   _AH = 5;
   _AL = page;
   geninterrupt ( VIDEO );
}

void vsetup ( void ) {
   byte omode = vgetmode ();
   if ( omode == 7 )
      vsetmode ( 7 );
   else {
      vsetmode ( 3 );
      vsetpage ( 0 );
   }
   vscroll( 0 );
}

void vlocate ( byte row, byte col ) {
   _AH = 2;
   _DH = row;
   _DL = col;
   _BH = 0;
   geninterrupt ( VIDEO );
   vrow = row; vcol = col;
}

int vputs ( char *string ) {
   char *old; old = string;
   while ( *string )
      vputch ( *string++ );
   return ( old - string );
}

void vbump ( void ) {
    vscroll( 1 );
}

void vscroll( int num)
{
   _AH = 6;	 /* scroll */
   _CH = vtop;
   _DH = vbot;
   _CL = vleft;
   _DL = vright;
   _BH = vatt;
   _AL = num;	 /* number of lines */
   geninterrupt ( VIDEO );
}

byte vputch ( byte c ) {
   extern char onConsole;

   vlocate ( vrow, vcol );
   switch ( c ) {
      case '\a' :
	if (onConsole) RunListA(&BellList, BellIt, NULL);
	break;
      case '\r' :
	vcol = vleft;
	break;
      case '\n' :
	if ( vrow == vbot )
	   vbump ( );
	else
	   vrow++;
	break;
      case '\t' : { int kount;
	for ( kount = 0; kount < 8; kount++ )
	   vputch ( ' ' ); }
	break;
      case '\b' :
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
	break;
      default :
	_AH = 9;
	_AL = c;
	_CX = 1;
	_BL = vatt;
	_BH = 0;
	geninterrupt ( VIDEO );
	if ( vcol == vright ) {
	   vcol = vleft;
	   if ( vrow == vbot )
	      vbump ( );
	   else
	      vrow++;
	}
	else
	   vcol++;
	break;
   }
   vlocate ( vrow, vcol );
   return ( c );
}

void statusline ( char *string ) {
   byte ocol = vcol,
	orow = vrow;
   int k;

   vlocate ( 1, vwherey );
      vatt = (ScrColors->StatBack << 4) + ScrColors->StatFore;
	 for ( k = vwherey; k < 80; k++ )
	    vputch ( ' ' );
	 vlocate ( 1, vwherey );
	 vputs ( string );
      vatt = (ScrColors->ScrBack << 4) + ScrColors->ScrFore;
   vlocate ( orow, ocol );
}

void BellIt(TwoNumbers *d, char *echo)
{
    sound(d->first);
    pause((int) d->second);
    nosound();
    if (echo != NULL) outMod(BELL);
}

void RestoreMasterWindow()
{
   vlocate(vrow,vcol);
}
