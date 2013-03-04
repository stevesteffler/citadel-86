#include "keystrk.h"

int KeyStroke()
{
        int c;

        if ((c = getch()) != 0) return c;
        switch (getch()) {
        case 'P': return DOWN;
        case 'M': return RIGHT;
        case 'K': return LEFT;
        case 'H': return UP;
        case 'S': return DEL_KEY;
        case 'G': return HOME_KEY;
        case 'O': return END_KEY;
        case 'R': return INS;
        case 'Q': return PG_DN;
        case 'I': return PG_UP;
	case ';': return F1_KEY;
	case '<': return F2_KEY;
	case '-': return ALT_X;
	case '=': return F3_KEY;
	case '>': return F4_KEY;
	case '?': return F5_KEY;
	case '@': return F6_KEY;
	case 'A': return F7_KEY;
	case 'B': return F8_KEY;
	case 'C': return F9_KEY;
	case 'D': return F10_KEY;
        }
        return -1;
}
