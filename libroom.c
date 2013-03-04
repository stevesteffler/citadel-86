/*
 *				libroom.c
 *
 * Library for room code.
 */

/*
 *				History
 *
 * 85Nov15 HAW  Created.
 */

#include "ctdl.h"

/*
 *				Contents
 *
 *	getRoom()		load given room into RAM
 *	putRoom()		store room to given disk slot
 */

aRoom	      roomBuf;	      /* Room buffer	      */
extern rTable *roomTab;	      /* RAM index	      */
extern CONFIG cfg;
FILE	      *roomfl;	      /* Room file descriptor     */
int	      thisRoom = LOBBY;      /* Current room	      */

/*
 * getRoom()
 *
 * Gets the designated room.
 */
void getRoom(int rm)
{
    long int s;

    /* load room #rm into memory starting at buf */
    thisRoom    = rm;
    s = (long) ((long) rm * (long) RB_TOTAL_SIZE);
    fseek(roomfl, s, 0);

    if (fread(&roomBuf, RB_SIZE, 1, roomfl) != 1)   {
	crashout(" ?getRoom(): read failed//error or EOF (1)!");
    }

    crypte(&roomBuf, RB_SIZE, rm);

    if (fread(roomBuf.msg, MSG_BULK, 1, roomfl) != 1)   {
	crashout(" ?getRoom(): read failed//error or EOF (2)!");
    }
}

/*
 * putRoom()
 *
 * stores room in buf into slot rm in ctdlroom.sys.
 */
void putRoom(int rm)
{
    long int s;

    s = (long) ((long) rm * (long) RB_TOTAL_SIZE);
    if (fseek(roomfl, s, 0) != 0) 
	crashout(" ?putRoom(): fseek failure!");

    crypte(&roomBuf, RB_SIZE, rm);

    if (fwrite(&roomBuf, RB_SIZE, 1, roomfl) != 1)   {
	crashout("?putRoom() crash!//0 returned!!!(1)");
    }

    if (fwrite(roomBuf.msg, MSG_BULK, 1, roomfl) != 1)   {
	crashout("?putRoom() crash!//0 returned!!!(2)");
    }

    crypte(&roomBuf, RB_SIZE, rm);
}

/*
 * noteRoom()
 *
 * This will enter a room into RAM index array.
 */
void noteRoom()
{
    int   i;
    MSG_NUMBER last;

    last = 0l;
#ifdef NORMAL_MESSAGES
    for (i = 0;  i < ((thisRoom == MAILROOM) ? MAILSLOTS : MSGSPERRM);  i++)  {
	if (roomBuf.msg[i].rbmsgNo > last && 
	    roomBuf.msg[i].rbmsgNo <= cfg.newest) {
	    last = roomBuf.msg[i].rbmsgNo;
	}
    }
#else
    for (i = 0;  i < ((thisRoom == MAILROOM) ? MAILSLOTS : MSGSPERRM);  i++)  {
	if ((roomBuf.msg[i].rbmsgNo & (~S_MSG_MASK)) &&
	    (roomBuf.msg[i].rbmsgNo & S_MSG_MASK) > cfg.oldest)
	    last = S_MSG_MASK;

	if ((roomBuf.msg[i].rbmsgNo & S_MSG_MASK) > last && 
	    (roomBuf.msg[i].rbmsgNo & S_MSG_MASK) <= cfg.newest) {
	    last = (roomBuf.msg[i].rbmsgNo & S_MSG_MASK);
	}
    }
#endif
    roomTab[thisRoom].rtlastMessage	= last;
    strcpy(roomTab[thisRoom].rtname, roomBuf.rbname) ;
    roomTab[thisRoom].rtgen		= roomBuf.rbgen  ;
    memcpy(&roomTab[thisRoom].rtflags, &roomBuf.rbflags,
						sizeof roomBuf.rbflags);
    roomTab[thisRoom].rtFlIndex   = roomBuf.rbFlIndex;
}
