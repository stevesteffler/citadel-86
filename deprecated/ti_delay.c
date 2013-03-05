#include <dos.h>

void delay(unsigned milliseconds)
{
register int x,c;
struct time clock_one;
struct time clock_two;

      c = (milliseconds + 1) / 10;
      for(x=0 ; x<=c ; x++)
      {
         gettime(&clock_one);
         for(;;) {
            gettime(&clock_two);
            if(clock_one.ti_hund == clock_two.ti_hund)
            continue;
            break;
         }
      }
}

