/*
 *				liblog2.c
 *
 * Citadel log code for the library
 */

/*
 *				history
 *
 * 89Feb05 HAW  File created.
 */

#include "ctdl.h"

/*
 *				Contents
 *
 * findPerson() 		Loads a log record.
 * PersonExists()		See if given name is valid for mail.
 */

extern CONFIG cfg;
extern logBuffer logTmp, logBuf;
extern LogTable  *logTab;
extern char onConsole, remoteSysop;

/*
 * findPerson()
 *
 * This function loads a log record for named person.
 * RETURNS: ERROR if not found, else log record #
 */
int findPerson(char *name, logBuffer *lBuf)
{
    int  h, i, foundIt, logNo;

    if (strLen(name) == 0) return ERROR;

    if (strCmpU(name, "Citadel") != SAMESTRING) {
        h   = hash(name);
        for (foundIt = i = 0;  i < cfg.MAXLOGTAB && !foundIt;  i++) {
            if (logTab[i].ltnmhash == h) {
                getLog(lBuf, logNo = logTab[i].ltlogSlot);
                if (lBuf->lbflags.L_INUSE && 
                        strCmpU(name, lBuf->lbname) == SAMESTRING) {
                    foundIt = TRUE;
                }
            }
        }
    }
    else foundIt = FALSE;
    if (!foundIt)    return ERROR;
    else             return logNo;
}

/*
 * PersonExists()
 *
 * This function will check to see if the given name is valid for mail.
 *
 * This includes special processing for "Sysop" and "Citadel".
 */
int PersonExists(char *name)
{
	int result;
	logBuffer temp;

	if ((strCmpU("Citadel", name) == SAMESTRING && HalfSysop()) ||
		             strCmpU("sysop", name) == SAMESTRING)
		return cfg.MAXLOGTAB;       /* signals special string */

	initLogBuf(&temp);
	result = findPerson(name, &temp);
	if (result != ERROR) strCpy(name, temp.lbname);
	killLogBuf(&temp);
	return result;
}
