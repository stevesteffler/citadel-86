void BellIt();
void dirExists(char disk, char *theDir);
void SetBaudTo(int x);
void InternalEnDis(char enable);
void mHangUp(void);
void InitPort(void);
void OutString(char *s);
char CheckArea(MenuId id, char c, char *drive, char *dir_x);
int  doAreaCommon(char *var, char *line, SYS_AREA *area, int offset, int which);
void anyArea(char *var, char *line, char *disk, char *target);
void MSDOSparse(char *theDir, char *drive);
void OpenDoorFile(void);
void StoreDoor(char *line, FILE *fd);
AN_UNSIGNED interpret(int instruction);
char *GetDoorData(char *line, char *field, int size);
char *NextWhite(char *line);
int CitSystem(char RestoreVideo, char *format, ...);
char *NextNonWhite(char *line);
void WrtDoor();
void WriteDoors(void);
int  ScreenColor(char *line);
int  findcolor(char *str);
char *NextWhite(char *line);
void DoDomainDirectory(int i, char kill);
char *NextNonWhite(char *line);
void *DoorName();
int waitPutch(int c);
char MakeCmdLine(char *target, char *source, char *miscdata, int len);
void InitExternEditors(void);
void InitDoors(void);
void *ChPhrase();
int SearchFileComments(char *FileName);
int BaudCode(int bps);
void InitProtocols(void);
char realSetSpace(char disk, char *dir);
void MSDOSparse(char *theDir, char *drive);
void doSendWork(char *filename, void (*fn)(DirEntry *fn));
int getModemId(void);
char ShowDoors(FILE *fd);
void initDirList(void);
int nodie(void);
int Control_C(void);
void SysWork(void (*form)(), char *cmdLine);
int specCmpU(char *f1, char *f2);
void setup_nocccb(void);
void VideoInit(void);
int fileType(char *drive, char dir);
int goodArea(MenuId id, char *prompt, char *dir, char *drive);
void AddName(DirEntry *fn);

        /* IBM support function prototypes */
void setInterrupts(void);
void killIBMint(void);

        /* IBM video function prototypes (see CITVID.H) */
#ifndef VMODULE
unsigned char vputch(unsigned char c);
#endif
void video(char *tagline);
void statusline(char *tagline);
void RestoreMasterWindow(void);

        /* Z-100 support function prototypes */
int mGetch(void);
int mHasch(void);
int mPutch(int c);
int mHasout(void);
int mInit(int port, int baud, int parity, int stopbits, int wordlen, int xon);

void Zreset_video(void);
void Zsline(char *data);
void Zvideo(char *tagline);
void Zreset_video(void);
void mClose(void);

        /* Miscellaneous prototypes */
void diskSpaceLeft(char drive, long *sectors, long *bytes);
int  check_CR(void);


#ifdef TIMER_FUNCTIONS_NEEDED
typedef struct {
        long tPday, tPhour, tPminute, tPsecond, tPmilli;
} TimePacket;

        /* Timer support function prototypes */
long timeSince(TimePacket *Slast);
long milliTimeSince(TimePacket *Slast);

void setTimer(TimePacket *Slast);

#endif

void SpaceBug(int x);
/* end of this file */
