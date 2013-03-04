/*
 *				splitf.c
 *
 * Splitf source.  Independent so we can access it from utilities.
 */

/*
 *				history
 *
 * 95Nov17 HAW  Created.
 */

#include "ctdl.h"
#include "ctype.h"
#include "time.h"
#include "stdarg.h"

/*
 * splitF()
 *
 * This formats format+args to file and console.
 */
void splitF(FILE *diskFile, char *format, ...)
{
    va_list argptr;
    char garp[MAXWORD];

    va_start(argptr, format);
    vsprintf(garp, format, argptr);
    va_end(argptr);
    printf(garp);

#ifdef NEEDED
if (strLen(garp) > MAXWORD) {
    killConnection("splitF");
    exit(3);
}
#endif

    if (diskFile != NULL) {
	fprintf(diskFile, garp);
	fflush(diskFile);
    }
}
