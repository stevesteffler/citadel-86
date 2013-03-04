/*
 *				libtime.c
 *
 * Random time functions.
 */

/*
 *				history
 *
 * 93Jan17 HAW	Created from misc.c.
 */

#include "ctdl.h"

/*
 *				contents
 *
 *	civTime()		MilTime to CivTime
 */

/*
 * civTime()
 *
 * Military time to Civilian time.
 */
void civTime(int *hours, char **which)
{
    if (*hours >= 12)
	*which = "pm";
    else
	*which = "am";
    if (*hours >= 13)
	*hours -= 12;
    if (*hours == 0)
	*hours = 12;
}

/*
 * Current_Time()
 *
 * This function will get the current time, format cutely.
 */
char *Current_Time()
{
    char  *ml, *month;
    int   year, day, h, m;
    static char Time[13];

    getCdate(&year, &month, &day, &h, &m);
    civTime(&h, &ml);
    sprintf(Time, "%d:%02d %s", h, m, ml);
    return Time;
}

/*
 * formDate()
 *
 * This function forms the current date.
 */
char *formDate()
{
    static char dateLine[40];
    int  day, year, h, m;
    char *month;

    getCdate(&year, &month, &day, &h, &m);
    sprintf(dateLine, "%d%s%02d", year, month, day);
    return dateLine;
}

char  *monthTab[13] = {"", "Jan", "Feb", "Mar",
			   "Apr", "May", "Jun",
			   "Jul", "Aug", "Sep",
			   "Oct", "Nov", "Dec" };
/*
 * getCdate()
 *
 * This retrieves system date and returns in the parameters.
 */
void getCdate(int *year, char **month, int *day, int *hours, int *minutes)
{
    int mon, seconds, milli;

    getRawDate(year, &mon, day, hours, minutes, &seconds, &milli);
    *year -= 1900;
    *month = monthTab[mon];
}

