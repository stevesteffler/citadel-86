/*
 *				compact.c
 *
 * message compaction / encryption
 */

#include "ctdl.h"

/*
 *				history
 *
 * 89Jun05 HAW  Created.
 */

/*
 *				Contents
 *
 *	StartEncode()		Start baudot-v encoding.
 *	StopEncode()		Stop same.
 *	Encode()		Does actual encoding of a byte.
 *	Combine()		Work function for encoding.
 *	StartDecode()		Start Decoding.
 *	StopDecode()		Stop Decoding.
 *	Decode()		Actual decoding function.
 *	Decode2()		Work function.
 */

/* #define DEBUG */

void StopEncode(void);
int Combine(int val);
int Decode2(int val);

#define NO_SET		4	/* no such set */
#define CAPS		0
#define SPEC_CHARS	3
#define COMP_SPACE	26	/* universal */
#define ZERO_COMP	16
#define RET_COMP	17

static struct {
	char Set,		   /* code set */
	CompValue;	 /* position in code set */
} compress[] = {
	{ 0, 0 },  { 2, 10 }, { 2, 11 }, { 2, 12 }, { 2, 13 }, /* ' ' - '$' */
	{ 2, 14 }, { 2, 15 }, { 2, 16 }, { 2, 17 }, { 2, 18 }, /* '%' - ')' */
	{ 2, 19 }, { 2, 20 }, { 2, 21 }, { 2, 22 }, { 2, 23 }, /* '*' - '.' */
	{ 2, 24 }, { 2, 0 },  { 2, 1 },  { 2, 2 },  { 2, 3 },  /* '/' - '3' */
	{ 2, 4 },  { 2, 5 },  { 2, 6 },  { 2, 7 },  { 2, 8 },  /* '4' - '8' */
	{ 2, 9 },  { 2, 25 }, { 3, 0 },  { 3, 1 },  { 3, 2 },  /* '9' - '=' */
	{ 3, 3 },  { 3, 4 },  { 3, 5 },  { 0, 0 },  { 0, 1 },  /* '>' - 'B' */
	{ 0, 2 },  { 0, 3 },  { 0, 4 },  { 0, 5 },  { 0, 6 },  /* 'C' - 'G' */
	{ 0, 7 },  { 0, 8 },  { 0, 9 },  { 0, 10 }, { 0, 11 }, /* 'H' - 'L' */
	{ 0, 12 }, { 0, 13 }, { 0, 14 }, { 0, 15 }, { 0, 16 }, /* 'M' - 'Q' */
	{ 0, 17 }, { 0, 18 }, { 0, 19 }, { 0, 20 }, { 0, 21 }, /* 'R' - 'V' */
	{ 0, 22 }, { 0, 23 }, { 0, 24 }, { 0, 25 }, { 3, 6 },  /* 'W' - '[' */
	{ 3, 7 },  { 3, 8 },  { 3, 9 },  { 3, 10 }, { 3, 11 }, /* '\' - '`' */
	{ 1, 0 },  { 1, 1 },  { 1, 2 },  { 1, 3 },  { 1, 4 },  /* 'a' - 'e' */
	{ 1, 5 },  { 1, 6 },  { 1, 7 },  { 1, 8 },  { 1, 9 },  /* 'f' - 'i' */
	{ 1, 10 }, { 1, 11 }, { 1, 12 }, { 1, 13 }, { 1, 14 }, /* 'j' - 'o' */
	{ 1, 15 }, { 1, 16 }, { 1, 17 }, { 1, 18 }, { 1, 19 }, /* 'p' - 't' */
	{ 1, 20 }, { 1, 21 }, { 1, 22 }, { 1, 23 }, { 1, 24 }, /* 'u' - 'y' */
	{ 1, 25 }, { 3, 12 }, { 3, 13 }, { 3, 14 }, { 3, 15 }  /* 'z' - '~' */
};

static char EncCurSet;
static int  BitsUsed, SendVal, Next, EncodeByteCount;
int (*EncOF)(int c);

int StartEncode(int (*func)(int c))
{
	BitsUsed = 0;
	EncCurSet   = NO_SET;
	EncOF	= func;
	SendVal  = 0;
	EncodeByteCount = 0;
	return 1;
}

void StopEncode()
{
	if (EncodeByteCount == 0) return;	/* don't bother */
	Combine(31);
	Combine(0);
	Combine(0);
	Combine(0);
}

int Encode(int c)
{
	int toReturn = TRUE;

	EncodeByteCount++;
	switch (c) {
	case ' ':   /* space exists in all code sets */
		if (EncCurSet == NO_SET) {
			Combine(CAPS + 27);
			EncCurSet = CAPS;
		}
		toReturn = Combine(COMP_SPACE); break;
	case 0:
	case '\r':
		if (EncCurSet != SPEC_CHARS) {
			Combine(SPEC_CHARS + 27);
			EncCurSet = SPEC_CHARS;
		}
		toReturn = Combine((c == 0) ? ZERO_COMP : RET_COMP);
		break;
	default:
				/* discard if not printable, C/R, or 0 byte */
		if (c >= ' ' && c < '~') {
			if (compress[c - 32].Set != EncCurSet) {
				Combine(compress[c - 32].Set + 27);
				EncCurSet = compress[c - 32].Set;
			}
			toReturn = Combine(compress[c - 32].CompValue);
		}
	}
	return toReturn;
}

static int Combine(int val)
{
	int		toReturn;

	SendVal |= (val << BitsUsed);
	SendVal &= 0xff;
	Next = (val >> BitsUsed);
	BitsUsed += 5;
	if (BitsUsed >= 8) {
		toReturn = (*EncOF)(SendVal);
		SendVal = val >> (13 - BitsUsed);
		BitsUsed %= 8;
	}
	else toReturn = TRUE;

	return toReturn;
}

void StopDecode(void);

#define NO_CAPS		1
#define NUMERICS	2
#define FINISHED	4	/* finished with encoded data */
#define INIT_SET	5	/* an initial value */

static char Numerics[] = {
		'0', '1', '2', '3', '4',
		'5', '6', '7', '8', '9',
		'!', '\"', '#', '$', '%',
		'&', '\'', '(', ')', '*',
		'+', ',', '-', '.', '/',
		':'
};

static char SpecChars[] = {
		';', '<', '=', '>', '?',
		'@', '[', '\\', ']', 0x5e,
		'_', '`', '{', '|', '}',
		'~', 0, '\r'
};

static char DecCurSet;
static int  ThisVal, NB;
int (*DecOF)(int val);

int StartDecode(int (*func)())
{
	ThisVal = 0;
	DecCurSet  = INIT_SET;
	DecOF   = func;
	NB	  = 5;
	return 1;
}

void StopDecode()
{
}

int Decode(int c)
{
	static int mask[] = {
	   0, 1, 3, 7, 0xf, 0x1f
	};
	int AB;

	if (DecCurSet == FINISHED) return TRUE;


	if (!Decode2(c & mask[NB])) return FALSE;

	c  >>= NB;

	AB = 8 - NB;

	if (AB >= 5) {
		ThisVal = 0;
		AB -= 5;
		NB  = 5;
		if (!Decode2(c & mask[NB])) return FALSE;
		c >>= 5;
	}
	NB = 5 - AB;
	ThisVal = c;
	return TRUE;
}

static int Decode2(int val)
{
	int toReturn;
	char Used;

	if (DecCurSet == FINISHED) return TRUE;
	ThisVal += (val << (5 - NB));
	switch (ThisVal - 27) {
		case CAPS:
		case NO_CAPS:
		case NUMERICS:
		case SPEC_CHARS:
		case FINISHED:
			DecCurSet = ThisVal - 27;
			Used = TRUE;
			toReturn = TRUE;
			break;
		default: Used = FALSE;
	}
		/* Discard if no set defined. */
	if (!Used) {
		if (DecCurSet == INIT_SET) toReturn = TRUE;

			/* True for any valid code set */
		else if (ThisVal == COMP_SPACE) toReturn = (*DecOF)(' ');
		else switch (DecCurSet) {
			case CAPS:
				toReturn = (*DecOF)('A' + ThisVal); break;
			case NO_CAPS:
				toReturn = (*DecOF)('a' + ThisVal); break;
			case NUMERICS:
				toReturn = (*DecOF)(Numerics[ThisVal]); break;
			case SPEC_CHARS:
				toReturn = (*DecOF)(SpecChars[ThisVal]); break;
		}
	}
	return toReturn;
}
