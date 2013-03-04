/*
 *				libnet.c
 *
 * Library net functions.
 */

/*
 *				history
 *
 * 85Nov15 HAW  Created.
 */

#include "ctdl.h"

NetBuffer	 netBuf;
NetBuffer	 netTemp;
extern NetTable  *netTab;
extern CONFIG    cfg;
int		 thisNet = -1;
FILE		 *netfl;

/*
 *				contents
 *
 *	getNet()		gets a node from CTDLNET.SYS
 *	putNet()		puts a node to CTDLNET.SYS
 *	searchNet()		search net on ID
 *	searchNameNet()		search net on Name
 */

extern FILE *netLog;

/*
 * getNet()
 *
 * This function gets a net node from the file.
 */
void getNet(int n, NetBuffer *buf)
{
    long int r, s;
    char error[50];

    if (buf == &netBuf)
	thisNet = n;
    r = NB_TOTAL_SIZE;
    s = n * r;

    fseek(netfl, s, 0);
    if (fread(buf, NB_SIZE, 1, netfl) != 1) {
	sprintf(error, "?getNet-read fail(1) for slot %d.", n);
	crashout(error);
    }

    crypte(buf, NB_SIZE, n);
}

/*
 * putNet()
 *
 * This will put a node to file.  It also updates the net index.
 */
void putNet(int n, NetBuffer *buf)
{
    long int r, s;
    label temp;
    char error[50];

    if (buf == &netBuf) thisNet = n;

    copy_struct(buf->nbflags, netTab[n].ntflags);
    netTab[n].ntnmhash = hash(buf->netName);
    normId(buf->netId, temp);
    netTab[n].ntidhash = hash(temp);
    netTab[n].ntMemberNets = buf->MemberNets;
    strCpy(netTab[n].ntShort, buf->nbShort);
    netTab[n].ntGen = buf->nbGen;

    r = NB_TOTAL_SIZE;
    s = n * r;
    crypte(buf, NB_SIZE, n);

    fseek(netfl, s, 0);

    if (fwrite(buf, NB_SIZE, 1, netfl) != 1) {
	sprintf(error, "?putNet-write fail(1), slot %d!!", n);
	crashout(error);
    }

    crypte(buf, NB_SIZE, n);
}

/*
 * searchNet()
 *
 * This function Searches the net for the given Id.
 */
int searchNet(char *forId, NetBuffer *buf)
{
    int   rover;
    label temp, temp2;

    normId(forId, temp2);
    for (rover = 0; rover < cfg.netSize; rover++) {
	if (netTab[rover].ntflags.in_use &&
	    hash(temp2) == netTab[rover].ntidhash) {
	    getNet(rover, buf);
	    normId(buf->netId, temp);
	    if (strCmpU(temp, temp2) == SAMESTRING)
		return rover;
	}
    }
    return ERROR;
}

/*
 * searchNameNet()
 *
 * This function will search the  net for given node name.  This includes checks
 * for both full and shorthand names.
 */
int searchNameNet(label name, NetBuffer *buf)
{
    int rover;

    if (strlen(name) == 0) return ERROR;

    /* First check for full name */
    for (rover = 0; rover < cfg.netSize; rover++) {
	if (netTab[rover].ntflags.in_use &&
	    hash(name) == netTab[rover].ntnmhash) {
	    getNet(rover, buf);
	    if (strCmpU(buf->netName, name) == SAMESTRING)
		return rover;
	}
    }

    /* Now check for shorthand name */
    for (rover = 0; rover < cfg.netSize; rover++)
	if (netTab[rover].ntflags.in_use &&
	    strCmpU(netTab[rover].ntShort, name) == SAMESTRING) {
	    getNet(rover, buf);
	    return rover;
	}

    return ERROR;
}

