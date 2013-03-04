/*
 * AnsiSys.H
 *
 * This file contains the prototypes for the interface to the system
 * dependencies.  If you need prototypes for some internal system
 * dependent code, use SysDep.H.  This file is no longer the domain of
 * the system dependent porter.  May 12, 1990.
 */

        /*
         * Use these only if we are ANSI capable...
         */
#ifdef ANSI_PROTOTYPING

#ifdef FAX_DEBUG
void AddFaxResults(void);
#endif

/*
 * These functions are the interface calls to the system dependencies.
 * Since some of these functions can be implemented as parameterized
 * defines, you must go through these and comment out those that should
 * not be here.
 */

/*
 * 3.1. Modem stuff
 */

#ifndef ModemOpen
void ModemOpen(char FromDoor);
#endif

#ifndef Citinp
AN_UNSIGNED Citinp(void);
#endif

#ifndef MIReady
int MIReady(void);
#endif

#ifndef ModemShutdown
void ModemShutdown(char KillCarr);
#endif

#ifndef outMod
char outMod(int c);
#endif

#ifndef fastMod
char fastMod(int c);
#endif

#ifndef ModemPushBack
void ModemPushBack(int c);
#endif

#ifndef gotCarrier
int gotCarrier(void);
#endif

#ifndef changeBauds
int changeBauds(MenuId id);
#endif

#ifndef DisableModem
void DisableModem(char FromNet);
#endif

#ifndef EnableModem
void EnableModem(char FromNet);
#endif

#ifndef ReInitModem
void ReInitModem(void);
#endif

#ifndef getNetBaud
char getNetBaud(void);
#endif

#ifndef ResultVal
int  ResultVal(char *buf);
#endif

#ifndef setNetCallBaud
int setNetCallBaud(int targetBaudCode);
#endif

#ifndef HangUp
void HangUp(char FromNet);
#endif

#ifndef BufferingOn
void BufferingOn(void);
#endif

#ifndef BufferingOff
void BufferingOff(void);
#endif

#ifndef Reinitialize
void Reinitialize(void);
#endif

/*
 * 3.2. Disk Stuff
 */

#ifndef mvToHomeDisk
int mvToHomeDisk(int x);
#endif

#ifndef RoomLeft
long RoomLeft(int room);
#endif

#ifndef RottenDial
int RottenDial(char *callout_string);
#endif

/*
 * 3.3. Console Stuff
 */

#ifndef getCh
int getCh(void);
#endif

#ifndef KBReady
char KBReady(void);
#endif

#ifndef mputChar
void mputChar(char c);
#endif

#ifndef simpleGetch
int simpleGetch(void);
#endif

#ifndef ScreenUser
void ScreenUser(void);
#endif

#ifndef ScrNewUser
void ScrNewUser(void);
#endif

#ifndef ScrTimeUpdate
void ScrTimeUpdate(int hr, int mn);
#endif

#ifndef SpecialMessage
void SpecialMessage(char *message);
#endif

#ifndef StopVideo
void StopVideo(void);
#endif

/*
 * 3.4. Area Stuff
 */

#ifndef CitGetFileList
void CitGetFileList(char *mask, SListBase *base, long before,
                                                long after, char *phrase);
#endif

#ifndef FindDirName
char *FindDirName(int roomno);
#endif

#ifndef ValidArea
char ValidArea(char *area);
#endif

#ifndef homeSpace
void homeSpace(void);
#endif

#ifndef netGetArea
int netGetArea(char *fn, struct fl_req *file_data, char ambiguous);
#endif

#ifndef netGetAreaV2
int netGetAreaV2(MenuId id, char *fn, struct fl_req *file_data, char ambiguous);
#endif

#ifndef netSetNewArea
char netSetNewArea(NET_AREA *file_data);
#endif

#ifndef prtNetArea
char *prtNetArea(NET_AREA *netArea);
#endif

#ifndef RoomSys
int RoomSys(int roomNo);
#endif

#ifndef SetSpace
char SetSpace(char *str);
#endif

#ifndef sysGetSendFilesV2
char sysGetSendFilesV2(MenuId id, char *name, struct fl_send *sendWhat);
#endif

#ifndef sysRoomLeft
long sysRoomLeft(void);
#endif

#ifndef sysSendFiles
void sysSendFiles(struct fl_send *sendWhat);
#endif

#ifndef updFiletag
void updFiletag(char *fileName, char *desc);
#endif

#ifndef makeAuditName
void makeAuditName(char *logfn, char *name);
#endif

#ifndef ValidDirFileName
char ValidDirFileName(char *fn);
#endif

#ifndef NormalName
void NormalName(struct fl_send *x, char *y);
#endif

#ifndef RedirectName
void RedirectName(char *buffer, char *directory, char *filename);
#endif

#ifndef MakeDomainDirectory
void MakeDomainDirectory(int dir);
#endif

#ifndef KillDomainDirectory
void KillDomainDirectory(int dir);
#endif

#ifndef MakeDomainFileName
void MakeDomainFileName(char *buffer, int Dir, char *filename);
#endif

#ifndef MoveFile
void MoveFile(char *oldname, char *newname);
#endif

#ifndef CopyFile
char CopyFile(char *oldname, int room, long *size);
#endif

#ifndef CopyFileGetComment
int CopyFileGetComment(char *fname, int thisRoom, char *comment);
#endif

#ifndef CopyFileToFile
void CopyFileToFile(char *fn, char *vfn);
#endif

#ifndef VirtualCopyFileToFile
void VirtualCopyFileToFile(char *src, char *target);
#endif

#ifndef ChangeToCacheDir
int ChangeToCacheDir(char *x);
#endif

#ifndef ToTempArea
void ToTempArea(void);
#endif

#ifndef KillTempArea
void KillTempArea(void);
#endif


/* 
 * 3.5. Baud handling
 */

#ifndef FindBaud
UNS_32 FindBaud(void);
#endif

/*
 * 3.6. File Stuff
 */

#ifndef makeHelpFileName
void makeHelpFileName(char *new, char *original);
#endif

#ifndef makeSysName
void makeSysName(SYS_FILE target, char *name, SYS_AREA *area);
#endif

#ifndef SysArea
void SysArea(char *buf, SYS_AREA *area);
#endif

#ifndef InitBio
void InitBio(void);
#endif

#ifndef MakeBioName
void MakeBioName(SYS_FILE target, char *name);
#endif

#ifndef MoveToBioDirectory
void MoveToBioDirectory(void);
#endif

#ifndef safeopen
FILE *safeopen(char *fn, char *mode);
#endif

#ifndef totalBytes
void totalBytes(long *size, FILE *fd);
#endif

/*
 * 3.7. System Formatting Functions
 */
#ifndef mPrintf
int  mPrintf(char *format, ...);
#endif

#ifndef NetPrintf
int NetPrintf(int (*method)(int c), char *format, ...);
#endif

#ifndef dPrintf
void dPrintf(char *format, ...);
#endif

#ifndef splitF
void splitF(FILE *diskFile, char *format, ...);
#endif

#ifndef mTrPrintf
void mTrPrintf(char *format, ...);
#endif

#ifndef ToFile
void ToFile(char *format, ...);
#endif

/*
 * 3.8. Timers
 */
#ifndef MilliSecPause
void MilliSecPause(int x);
#endif

#ifndef pause
void pause(int i);
#endif

#ifndef startTimer
void startTimer(int TimerId);
#endif

#ifndef getRawDate
void getRawDate(int *year, int *month, int *day, int *hours, int *minutes,
                        int *seconds, int *milli);
#endif

#ifndef setRawDate
char setRawDate(int year, int month, int day, int hour, int min);
#endif

#ifndef AbsToReadable
char *AbsToReadable(unsigned long lastdate);
#endif

#ifndef ReadDate
int  ReadDate(char *date, long *RetTime);
#endif

#ifndef CurAbsolute
long CurAbsolute(void);
#endif

#ifndef chkTimeSince
long chkTimeSince(int TimerId);
#endif

/*
 * 3.9. Miscellaneous
 */

#ifndef CheckForFax
void CheckForFax(void);
#endif

#ifndef CallChat()
void CallChat(int limit, int flags);
#endif

#ifndef copy_struct
void copy_struct(char *src, char *dest);
#endif

#ifndef copy_array
void copy_array(char *src, char *dest);
#endif

#ifndef copy_ptr
void copy_ptr(char *src, char *dest, int s);
#endif

#ifndef CheckSystem
char CheckSystem(void);
#endif

#ifndef GiveSpaceLeft
void GiveSpaceLeft(int thisRoom);
#endif

#ifndef systemCommands
void systemCommands(void);
#endif

#ifndef systemShutdown
void systemShutdown(int SysErrorVal);
#endif

#ifndef OutsideEditor
void OutsideEditor(void);
#endif

#ifndef OtherEditOptions
void OtherEditOptions(char **Options);
#endif

#ifndef ShowOutsideEditors
void ShowOutsideEditors(void);
#endif

#ifndef RunRemoteEditor
void RunRemoteEditor(int s);
#endif

#ifndef receive
int receive(int seconds);
#endif

#ifndef WhatDay
int WhatDay(void);
#endif

#ifndef systemInit
int systemInit(void);
#endif

#ifndef ResIntrp
void *ResIntrp(char *line);
#endif

#ifndef zero_struct
void zero_struct(void *target);
#endif

#ifndef zero_array
void zero_array(char *target);
#endif

#ifndef ClearDoorTimers
void ClearDoorTimers(void);
#endif

#ifndef Cumulate
void Cumulate();
#endif

#ifndef BackFromDoor
char BackFromDoor(void);
#endif

#ifndef doDoor
char doDoor(char moreYet);
#endif

#ifndef DoorHelpListing
void DoorHelpListing(char *target);
#endif

#ifndef NewUserDoor
char NewUserDoor(void);
#endif

#ifndef LoggedInDoor
char LoggedInDoor(void);
#endif

#ifndef RunAutoDoor
char RunAutoDoor(int i, char ask);
#endif

#ifndef ReadBps
void ReadBps(char *str);
#endif

#ifndef  SetUpPort
int  SetUpPort(int bps);
#endif

#ifndef BeNice
void BeNice(int x);
#endif

#ifndef DialExternal
int DialExternal(NetBuffer *netBuf);
#endif

#ifndef ChatEat
char ChatEat(int c)
#endif

#ifndef ChatSend
char ChatSend(int c)
#endif

#ifndef makeBanner
void makeBanner(char *x, char *y, int z);
#endif

/*
 * 3.11 File Comments
 */
#ifndef StFileComSearch
int StFileComSearch(void);
#endif

#ifndef FindFileComment
int FindFileComment(char *fn, char extraneous);
#endif

#ifndef EndFileComment
void EndFileComment(void);
#endif


/*
 * 3.12 deARCing prototypes.
 */
#ifndef ArcInit
void ArcInit(void);
#endif

#ifndef SendArcFiles
void SendArcFiles(int protocol);
#endif

#ifndef MakeTempDir
void MakeTempDir(void);
#endif

#ifndef FileIntegrity
char FileIntegrity(char *fileName);
#endif

#ifndef CompAvailable
char CompAvailable(char CompType);
#endif

#ifndef DeCompAvailable
char DeCompAvailable(char CompType);
#endif

#ifndef NetDeCompress
void NetDeCompress(char CompType, SYS_FILE fn);
#endif

#ifndef MakeDeCompressedFilename
void MakeDeCompressedFilename(SYS_FILE fn, char *FileName);
#endif

#ifndef KillNetDeCompress
void KillNetDeCompress(char *dir);
#endif

#ifndef Compress
void Compress(char CompType, char *Files, char *ArcFileName);
#endif

#ifndef CompExtension
char *CompExtension(char CompType);
#endif

#ifndef GetUserCompression
int GetUserCompression(void);
#endif

#ifndef GetCompEnglish
char *GetCompEnglish(char CompType);
#endif

#ifndef AnyCompression()
char AnyCompression(void);
#endif

#ifndef ReadExternalDir
void ReadExternalDir(char *name);
#endif

/*
 * 3.13 External protocols
 */
#ifndef RunExternalProtocol
int RunExternalProtocol(PROTOCOL *Prot, char *mask);
#endif

#ifndef ExternalTransfer
char ExternalTransfer(PROTOCOL *Prot, char *filename);
#endif

#ifndef LastOn
char *LastOn(long lastdate, char shortstyle);
#endif

#ifndef ReadSysProtInfo
SystemProtocol *ReadSysProtInfo(char *dup);
#endif

/*
 * Menu handling stuff
 */
#ifndef RegisterSysopMenu
MenuId RegisterSysopMenu(char *MenuName, char *Opts[],
						char *MenuTitle, int flags);
#endif

#ifndef GetSysopMenuChar
int GetSysopMenuChar(MenuId id);
#endif

#ifndef CloseSysopMenu
void CloseSysopMenu(MenuId id);
#endif

#ifndef SysopMenuPrompt
void SysopMenuPrompt(MenuId id, char *prompt);
#endif

#ifndef SysopError
void SysopError(MenuId id, char *prompt);
#endif

#ifndef SysopGetYesNo
char SysopGetYesNo(MenuId id, char *info, char *prompt);
#endif

#ifndef SysopRequestString
void SysopRequestString(MenuId id, char *prompt, char *buf, int size, int flags);
#endif

#ifndef SysopInfoReport
void SysopInfoReport(MenuId id, char *info);
#endif

#ifndef SysopDisplayInfo
void SysopDisplayInfo(MenuId id, char *info, char *title);
#endif

#ifndef SysopGetNumber
long SysopGetNumber(MenuId id, char *prompt, long bottom, long top);
#endif

#ifndef SysopContinual
MenuId SysopContinual(char *title, char *prompt, int Width, int Depth);
#endif

#ifndef SysopContinualString
char SysopContinualString(MenuId id, char *prompt, char *buf, int size, int flags);
#endif

#ifndef SysopCloseContinual
void SysopCloseContinual(MenuId id);
#endif

#ifndef SysopPrintf
int  SysopPrintf(MenuId id, char *format, ...);
#endif

#ifndef NeedSysopInpPrompt
char NeedSysopInpPrompt(void);
#endif

/*
 * Caching functions -- possibly temporary
 */

#ifndef NetCacheName
void NetCacheName(char *buf, int slot, char *name);
#endif

#ifndef MakeNetCacheName
void MakeNetCacheName(char *buf, int slot);
#endif

#ifndef MakeNetCache
void MakeNetCache(char *buf);
#endif

/*
 * Virtual rooms
 */

#ifndef makeVASysName
void makeVASysName(char *x, char *y);
#endif

#ifndef CreateVAName
void CreateVAName(char *fn, int slot, char *dir, long num);
#endif

/*
 * These functions are in SysCfg.C.
 */

#ifdef CONFIGURE

#ifndef initSysSpec
void initSysSpec(void);
#endif

#ifndef SysWildCard
void SysWildCard(void (*fn)(DirEntry *amb), char *mask);
#endif

#ifndef sysArgs
char sysArgs(char *str);
#endif

#ifndef sysSpecs
int sysSpecs(char *line, int offset, char *status, FILE *fd);
#endif

#ifndef SysDepIntegrity
char SysDepIntegrity(int *offset);
#endif

#ifndef FinalSystemCheck
int FinalSystemCheck(char OnlyParams);
#endif

#ifndef FindDoorSlot
int FindDoorSlot(char *str);
#endif

#ifndef doArea
int doArea(char *var, char *line, SYS_AREA *area, int offset);
#endif

#ifndef AreaCheck
int AreaCheck(SYS_AREA *area);
#endif

#ifndef GetDir
char *GetDir(SYS_AREA *area);
#endif

#ifndef GetDomainDirs
void GetDomainDirs(SListBase *list);
#endif

#endif
/*
 * These functions are in SysUtil.C.
 */

#ifndef getUtilDate(int *year, int *month, int *day, int *hours, int *minutes);
void getUtilDate(int *year, int *month, int *day, int *hours, int *minutes);
#endif

#include "sysprot.h"

#else   /* !Ansiprototyping */

FILE *safeopen();

long CurAbsolute(),
     sysRoomLeft(),
     chkTimeSince();

AN_UNSIGNED Citinp();

char *prtNetArea();

#ifdef SYSTEM_DEPENDENT

int  nodie(),
     Control_C(),
     video(),
     statusline(),
     Zvideo(),
     Zsline(),
     Zreset_video(),
     AddName();

char *gcdir(),

AN_UNSIGNED interpret();

long milliTimeSince(),
     timeSince(),
     dostounix();

#ifdef CONFIGURE

char *getcwd();

#endif

#endif

#endif
