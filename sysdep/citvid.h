typedef unsigned char byte;

#define VIDEO 16

#ifdef VMODULE
#   define SCOPE /* */
#else
#   define SCOPE extern
#endif

SCOPE byte vatt,
           vrow, vcol, vtop,      /* (autoset to 0) */
           vbot         = 24,
           vleft        = 0,
           vright       = 79,
           vwherey      = 18;

void vscroll ( int num );
void vsetmode ( byte mode );
byte vgetmode ( void );
void vsetpage ( byte page );
void vsetup ( void );
void vlocate ( byte row, byte col );
int  vputs ( char *string );

void video ( char *tagline );
void statusline ( char *string );

byte vputch ( byte c );
