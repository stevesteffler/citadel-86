/*
 *				libCryp.c
 *
 * Library of encryption (std) for Citadel.
 */

/*
 *				history
 *
 * 91????? HAW	Hash moved to Tools.c.
 * 87Aug06 HAW  Hash added.
 * 85Nov15 HAW  Created.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	crypte()		encrypts/decrypts data blocks
 */

extern CONFIG cfg;		/* Configuration variables      */

/*
 * crypte()
 *
 * encrypts/decrypts data blocks
 *
 * This was at first using a full multiply/add pseudo-random sequence
 * generator, but 8080s don't like to multiply.  Slowed down I/O
 * noticably.  Rewrote for speed.
 *
 * 84Sep04 HAW  I'll just use it...... 
 */
void crypte(void *buf, unsigned len, unsigned seed)
{
    static AN_UNSIGNED *b;      /* Make this static for speed (I guess),*/
    static  int c, s;		/* since register variables not around  */

    seed	= (seed + cfg.cryptSeed) & 0xFF;
    b		= (AN_UNSIGNED *) buf;
    c		= len;
    s		= seed;
    for (;  c;  c--) {
	*b++   ^= s;
	s       = (s + CRYPTADD)  &  0xFF;
    }
}
