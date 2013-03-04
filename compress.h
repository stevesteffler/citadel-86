/*
 *				Compress.h
 *
 * #include file for data structures concerning compressed files.
 */

#ifndef _COMPRESSED

#define _COMPRESSED

/*
 *				History
 *
 * 92Nov22 HAW  Split off from ctdl.h.
 */

/*
 * Table for decoding funny files
 */
typedef struct {
	char *Format;
	char Many;
	char (*Func)(FILE *fd, ...);
} FunnyInfo;

extern FunnyInfo Formats[];

/*
 *			SEA ARC reading structure
 */
typedef struct {
    char   ArchiveMark;
    char   Header;
    char name [13];     /* file name */
    UNS_32 size;        /* size of compressed file */
    UNS_16 date;        /* file date*/
    UNS_16 time;        /* file time */
    UNS_16 crc;         /* cyclic redundancy check */
    UNS_32 length;      /* true file length */
} ARCbuf;

/*
 *			PK ZIP header structure
 */
typedef struct {
	UNS_32 Signature;
	UNS_16 ExtVersion;
	UNS_16 BitFlags;
	UNS_16 Method;
	UNS_16 FileTime;
	UNS_16 FileDate;
	UNS_32 CRC;
	UNS_32 CompSize;
	UNS_32 NormalSize;
	UNS_16 NameLength;
	UNS_16 FieldLength;
} ZipHeader;

/*
 *			ZOO header information
 *	These structures were taken directly from Rahul Dhesi's code
 *	and are under his copyright.
 */
#define SIZ_TEXT  20                   /* Size of header text */
#define FNAMESIZE 13                   /* Size of DOS filename */
#define LFNAMESIZE 256                 /* Size of long filename */
#define PATHSIZE 256                   /* Max length of pathname */
typedef struct {
    char text[SIZ_TEXT];
    UNS_32 zoo_tag;
    UNS_32 zoo_start;
    UNS_32 zoo_minus;
    char major_ver;
    char minor_ver;
    char type;
    UNS_32 acmt_pos;
    UNS_16 acmt_len;
    UNS_16 vdata;
} zoo_header;

typedef struct {
    UNS_32 zoo_tag;
    char type;
    char packing_method;
    UNS_32 next;
    UNS_32 offset;
    UNS_16 date;
    UNS_16 time;
    UNS_16 file_crc;
    UNS_32 org_size;
    UNS_32 size_now;
    char major_ver;
    char minor_ver;
    char deleted;
    char struc;
    UNS_32 comment;
    UNS_16 cmt_size;
    char fname[FNAMESIZE];
    UNS_16 var_dir_len;
    char tz;
    UNS_16 dir_crc;
    /* fields for variable part of directory entry follow */
    char namlen;
    char dirlen;
    char lfname[LFNAMESIZE];
    char dirname[PATHSIZE];
    UNS_16 system_id;
    UNS_32 fattr;
    UNS_16 vflag;
    UNS_16 version_no;
} zoo_direntry;

/*
 *			LZH structure information
 *			courtesy Daniel Durbin
 */
typedef struct {			/* Local file header */
	char unknown1[2];		/* ? */
	char method[5];			/* compression method */
	UNS_32 csize;			/* compressed size */
	UNS_32 fsize;			/* uncompressed size */
	UNS_16 ftime;			/* last mod file time (msdos format) */
	UNS_16 fdate;			/* last mod file date */
	char fattr;			/* file attributes */
	char unknown2;			/* ? */
	char namelen;			/* filename length */
} LZHead;

/*
 *			Gif header information
 */
typedef struct {
	char   Sig[6];	/* no null */
	UNS_16 Width;
	UNS_16 Height;
	char   Colors;
} GifHeader;

#endif
