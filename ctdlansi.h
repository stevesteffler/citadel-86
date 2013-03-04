	/*
	 * Use these only if we're (semi) ANSI capable, with prototypes.
	 */
#ifdef ANSI_PROTOTYPING

void *FindStr();
	/* These functions are located in CTDL.C */

#ifdef USER_INTERFACE

char doChat(char expand, char first);
char doEnter(char expand, char first);
char doForget(char expand);
char doGoto(char expand);
char doHelp(char expand);
char doKnown(char expand);
char doLogin(char expand);
char doLogout(char expand, char first);
char doMeet(char moreYet);
char doRead(char expand, char first);
char doSkip(char expand);
char doUngoto(char expand);
char OptionCheck(char mode, int slot);
char doRegular(char x, char c);
char doSysop(void);
char getCommand(char *c, char bs);
char doAide(char moreYet, char first);

void doDirectory(char doDir, label fileName, char *phrase);
void UserAdmin(logBuffer *lBuf);
void greeting(void);

#endif

ValidateShowMsg_f_t OptionValidate;

	/* These functions are located in LOG.C */

#ifdef LOGIN

char doInviteDisplay(void);
void login(char *password);
void newPW(void);
void newUser(logBuffer *lBuf);
void storeLog(void);

#endif
void terminate(int discon, int save);
void slideLTab(int slot, int last);
void RemoveUser(int logNo, logBuffer *lBuf);

#ifdef INTERRUPTED_MESSAGES

char GetIntMessage(void);
char CheckIntMessage(void);

#endif

int DoAllQuestion(char *posprompt, char *netprompt);
int PWSlot(char pw[NAMESIZE], char load);
int findPerson(char *name, logBuffer *lBuf);
int GetUser(label who, logBuffer *lBuf, char Menu);

int strCmpU(char s[], char t[]);

	/* These functions are located in MSG.C */

#define B_HERE
void *FindIcky();
void *EatIcky(char *str);

char putMessage(logBuffer *lBuf, UNS_16 flags);
char hldMessage(char IsReply);
char findMessage(SECTOR_ID loc, MSG_NUMBER id, char ClearOthers);

int makeMessage(char uploading);
int showMessages(int flags, MSG_NUMBER LastMsg, ValidateShowMsg_f_t *);
void netMailOut(char isdomain, char *system, char *domain, char MsgBase,
							int slot, int flags);
char HandleControl(char doq);
char mAbort(void);
char redirect(char *name, int flags);
void undirect(void);
int SaveMessage(char IsReply);
void aideMessage(char *name, char noteDeletedMessage);
void mFormat(char *string, void (*)(char), char (*)(void));
void noteAMessage(theMessages *base, int slots, MSG_NUMBER id, SECTOR_ID loc);
ValidateShowMsg_f_t printMessage;
int procMessage(char uploading, char IsReply);
int FindNextFile(char *base);
void DelMsg(char killit, int m);
void msgToDisk(char *filename, char all, MSG_NUMBER id, SECTOR_ID loc,
								UNS_16 size);
void TranslateFilename(char *realfn, char *fn);
char getRecipient(void);
void dLine(char *garp);
char putWord(char *st, void (*)(char), char (*)(void));

#ifdef MSG_INTERNALS

void AddMail();			/* intentionally unfinished */
void MailWork(int slot);
void NetForwarding(logBuffer *lBuf);
void AddNetMail(char *system, int flags);
void doFlush(FILE *whichmsg, struct mBuf *mFile);
void fakeFullCase(char *text);
void flushMsgBuf(void);
void MakeIntoRouteMail(int result, DOMAIN_FILE fn, char isdomain, char *system,
			char *domain, char OriginIsMsgBase, int slot);
void mPeek(void);
void MsgShow(void);
void noteMessage(logBuffer *lBuf, UNS_16 flags);
void netMailProcess(int netPlace);
void CheckForwarding(logBuffer *lbuf);
void TellRoute(void);
void ShowReply(int i);

char InterruptMessage(void);
char deleteMessage(int m);
char canRespond(void);
char dGetWord(char *dest, int lim);
char doActualWrite(FILE *whichmsg, struct mBuf *mFile, char c);
char replyMessage(MSG_NUMBER msgNo, SECTOR_ID Loc);
char HasWritePrivs(void);
char idiotMessage(void);
char moveMessage(char answer, int m, char *toReturn);
char pullIt(int m);

int getWord(char *dest, char *source, int offset, int lim);
int putMsgChar(char c);
int DoRespond(SECTOR_ID loc, MSG_NUMBER msgNo);

#endif
void SetShowLimits(char rev, int *start, int *finish, int *increment);

	/* These functions are located in INFO.C */
void EditInfo(void);
char ReadCitInfo(void);
void WriteOutInformation(void);
char *GetInfo(label name);
void KillInfo(char *name);
void ChangeInfoName(char *newname);
void AllInfo(void);
char doInfo(void);

	/* These functions are located in MISC.C */
char ARCDir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr);
char ZIPDir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr);
char ZOODir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr);
char LZHDir(FILE *fd, char *FileName, long *RSize, long *SSize, char *DateStr);
char *PrintPretty(long s, char *result);
char GifDir(FILE *fd, char longexpl, char *buf);
void ShoveCR(void);
char *Current_Time(void);
char reconfigure(void);
int  CompressType(char *name);
char MoreWork(char AtMsg);
void RestorePointers(MSG_NUMBER	*msgptrs);
int AndMoreWork(int debug);
char configure(logBuffer *lBuf, char AllQuestions, char AllowAbort);
void PrepareForMessageDisplay(char);

void DosToNormal(char *DateStr, UNS_16 DosDate);
void changeDate(void);
void civTime(int *hours, char **which);
long FileCommentUpdate(char *fileName, char aideMsg, char ask);
void crashout(char *message);
void DirFree();
void *DirCheck();
int  DirCmp();
char doCR(void);
void ExtraOption(char *Opts[], char *NewOpt);
void download(int msgflags, char protocol, char global, int Compression,
					OfflineReader *Reader);
void doGlobal(int flags, ValidateShowMsg_f_t * func);
void RmTempFiles(char *dir);
void getCdate(int *year, char **month, int *day, int *hours, int *minutes);
char ingestFile(char *name, char *msg);
void NormStr(char *st);
void SaveInterrupted(MessageBuffer *SomeMsg);
void PagingOn(void);
void TranFiles(int protocol, char *phrase);
void UpdateForwarding(void);
void *FindSelect();
void TranSend(int protocol, void (*fn)(DirEntry *fn), char *filespec,
	char *phrase, char NeedToMove);
void CompressedDir(DirEntry *fn);
void doFormatted(DirEntry *fn);
void upLoad(int WC);
void writeTutorial(FILE *fd, char noviceWarning);

char *formHeader(char showtime);
char *formRoom(int roomNo, int showPriv, int noDiscrimination);
char *formDate(void);
char HelpIfPresent(char *filename);
char *lbyte(char *l);
char MultiBanner(char *basename);
char visible(AN_UNSIGNED c);
char CheckDLimit(long estimated);
char TranAdmin(int protocol, int NumFiles);

int AsciiHeader(long fileSize, char *filename);
int FindFunnyEntry(char *fn);
int CmdMenuList(char *Opts[], SListBase *Selects, char *HelpFile, char *List,
						char moreYet, char OneMore);

int GetMenuChar(void);
int GetSecond(void);
int NextSeq(void);
int putBufChar(int c);
int putFLChar(int c);

void transmitFile(DirEntry *filename);
void SendThatDamnFile(FILE *fbuf, int (*method)(int c));
void *FindSelect();

CRC_TYPE calcrc(unsigned char *ptr, int count);

	/* These functions are located in ROOMA.C */
void dumpRoom(char ShowFloor);
void fillMailRoom(void);
void retRoom(char *roomName);
void CountMsgs(int *count, int *NewCount);
void searchRooms(char *str);
void setUp(char justIn);
void systat(void);
void UngotoMaintain(int lRoom);
void SetKnown(int GenVal, int Room, logBuffer *lBuf);
void listRooms(char mode);
void SetMailRoom(void);

char initCitadel(void);
char CheckForSkippedMsgs(void);
char legalMatch(int i, label target);
char knowRoom(logBuffer *lBuf, int i);
char *DateSearch(char *fn, long *before, long *after);

void fDir(DirEntry *file);
int KnownRoom(int roomNo);
int RealGNR(char *nam, int (*func)(char *room));
int GotoNamedRoom(char *name, int flags);
int SkippedNewRoom(int i);
int gotoRoom(char *nam, int flags);
int tableRunner(int (*func)(), char OnlyKnown);
int partialExist(label target);
int roomCheck(int (*checker)(), char *nam);
int roomExists(char *room);
void ShowVerbose(DirEntry *fn);
int wildCard(void (*fn)(), char *filename, char *phrase, int flags);
int wild2Card(SListBase *Files, void (*fn)(), int flags);
int ReadMessageSpec(char *source, OptValues *opt);

	/* These functions are located in ROOMB.C */
char conGetYesNo(char *prompt);
char coreGetYesNo(char *prompt, int consoleOnly);
char *CleanEnd(char *msg);
char getText(int uploading);
char GetBalance(int uploading, char *buf, int size, char type, char *fn);
char getYesNo(char *prompt);
char *matchString(char *buf, char *pattern, char *bufEnd);
char renameRoom(void);
char *formatSummary(char *buffer, char NotFinal);
char
getXString(char *prompt, char *target, int targetSize, char *CR_str, 
						char *dft);
char getXInternal(MenuId id, char *prompt, char *target, int targetSize,
						char *CR_str, char *dft);
char SepNameSystem(char *string, char *name, char *system, NetBuffer *buf);
char WhoIsModerator(char *buf);

int OtherRecipients(char *name, int CCAddFlag);
int WritePrivs(char *name, int DoWritePrivs);
int CmnNetList(char *name, SharedRoomData **room, char ShouldBeThere,
								char *errstr);
int editText(char *buf, int lim, char MsgEntryType, char *fn);
int findRoom(void);
int knownHosts(char *name, int ShType);
int makeKnown(char *user, int arg);
int makeUnknown(char *user, int arg);
int doMakeWork(char *user, int val);
int killFromList(char *sysName, int arg);

SharedRoomData *searchForRoom(char *name);
SharedRoomData *ListAsShared(char *name);

long getNumber(char *prompt, long bottom, long top);

void CCDeliver(void *dd);
int getString(char *prompt, char *buf, int lim, int Flags);
int BlindString(char *buf, int lim, int Flags,
			int (*input)(void), void (*output)(char c), char Echo);
void givePrompt(void);
void indexRooms(void);
void insertParagraph(char *buf, int lim);
void makeRoom(void);
int getNormStr(char *prompt, char *s, int size, int Flags);
void noteRoom(void);
int getList(int (*fn)(char *data, int arg), char *prompt, int size,
							char Sysop, int arg);
void replaceString(char *buf, int lim, char Global);
void initialArchive(char *fn);
void WriteAList(SListBase *base, char *fn, void (*func)(NumToString *d));

	/* These functions are located in FLOORS.C */
int CheckFloor(int i);
int UserToFloor(char *name, int arg);
int FloorRunner(int start, int (*func)());
int NewRoom(int flags);
int FirstRoom(int FloorNo);
int RoomHasNew(int i);
int NSRoomHasNew(int i);
int DoFloors(void);
int Zroom(int i);
int FSroom(int i);
int FindAny(int i);
char FAide(void);
int MoveToFloor(char *name, int Floor);
int DispRoom(int i);
int MaimOrKill(int i);

void DispFloorName(int FloorNo);
void FInvite(void);
void FWithdraw(void);
void FSkipped(void);
char FForget(void);
char FConfigure(void);
char FGotoSkip(int mode);
void DeleteFloors(void);
void MoveRooms(void);
void RenameFloor(void);
void RunDisplay(void);
void CreateFloor(void);
void FlModerator(void);
void putFloor(int i);
void KillFloor(void);
char FKnown(char mode);

	/* These functions are located in EVENTS.C */
void ForceAnytime(void);
void InitEvents(char SetTime);
void ResolveDLStuff(void);
void InitEvTimes(void);
void setPtrs(void);
void EventShow(void);
void ActiveEvents(char *buf);
void *ChkTwoNumbers();
void *EatTwoNumbers(char *line);
void WrtTwoNumbers();

char DoTimeouts(void);
char HandleQuiet(int index);
char *ChkPreempt(long estimated);
long TimeToNextPreemptive(void);
char AlreadyProcessed(EVENT *s);
char *RedirectFile(char *filename, char *systemname);

long WeekDiff(long future, long now);

int CheckAutoDoor(char *name);
int during(EVENT *x);
int passed(EVENT *x);
int FigureEvent(int index);
int CmpED();
int CmpTwoLong();

TwoNumbers *MakeTwo(int First, long Second);

	/* These functions are in DOMAINS.C */
int UtilDomainOut(char (*f)(char *name, char *domain, char LocalCheck),
						char LocalCheck);
void WriteDomainContents(void);
void UpdateMap(void);
void RationalizeDomains(void);
void DomainInit(char FirstTime);
void UtilDomainInit(char FirstTime);
void DomainFileAddResult(char *DName, label system, label NodeId, char result);

char *LocalName(char *system);
char *RealDomainName(char *name);
char SystemInSecondary(char *Name, char *Domain, char *dup);
char DomainMailFileName(DOMAIN_FILE buffer, label DName, label NodeId,
						label NodeName);

UNS_16 FindCost(char *domain);

int DomainOut(char LocalCheck);

#ifdef CONFIGURE
	/* These functions are located in CONFG2.C */
void RoomInfoIntegrity(void);
void CheckBaseroom(void);
int EatEvent(char *line, int offset);
int GetStoreQuote(char *line, char *target, int *rover, int *offset);
int FigureDays(char *vals, UNS_32 *Days);

char *getLVal(char *line, int *rover, char fin);
void DomainIntegrity(void);

MULTI_NET_DATA FigureNets(char *str);

void EvIsDoor();
void EvFree();
void EventWrite();

	/* These functions are located in CONFG.C */
void init(int attended);
void readString(char *source, char *destination, char doProc);
void xlatfmt( char *s );
void illegal(char *errorstring);
void msgInit(void);
void indexRooms(void);
void noteRoom(void);
void logInit(void);
void noteLog(void);
void slideLTab(int slot, int last);
void wrapup(char onlyParams);
void netInit(void);
void crashout(char *str);
void CheckFloors(void);

char isoctal( int c );
char dGetWord(char *dest, int lim);
char zapMsgFile(void);
char realZap(void);
char zapRoomFile(void);
char zapLogFile(void);
char cfindMessage(SECTOR_ID loc, MSG_NUMBER id);

int logSort();
int msgSort();
int strCmpU(char s[], char t[]);

void FindHighestNative(MSG_NUMBER *all, MSG_NUMBER *bb);

#endif

	/* These functions are located in MODEM.C */
char BBSCharReady(void);
char getMod(void);
int iChar(void);
char JumpStart(int tries, int timeout, int Starter, int t1, int t2,
	char (*Method)(int (*wrt)(int c)), int (*WriteFn)(int c));
char Reception(int protocol, int (*WriteFn)(int c));
char recWX(int (*WriteFn)(int c));
char recXYmodem(int (*WriteFn)(int c));
char sendWXchar(int data);
char Transmission(int protocol, char mode);
char XYBlock(int mode, int size);

void FlowControl(void);
void GenTrInit(void);
void initTransfers(void);
void interact(char ask);
void interOut(char c);
void modemInit(int KillCarr);
void ChatGrab(char Up);
void oChar(char c);
void PushBack(char c);
char ModemSetup(char ShouldBeCarrier);
void ringSysop(void);
void runHangup(void);
void SendCmnBlk(char type, TransferBlock *block, char (*SendFn)(int c),
						int size);
void SummonSysop(void);
void WXResponses(void);

int ClearWX(void);
int CommonPacket(char type, int size, int (*recFn)(int t), int *Sector);
int CommonWrite(int (*WriteFn)(int c), int size);
int ExternalProtocol(int protocol, char up, char *name, char *phrase, 
                                                char move);
int SurreptitiousChar(char c);
void getSize(DirEntry *fileName);
int recWXchar(int ErrorTime);
int sendWCChar(int c);
int YMHdr(long fileSize, char *filename);
int sendWXModem(int c);
int sendYMChar(int c);
int sendAscii(int c);
int SendYBlk(void);
int XYClear(void);

AN_UNSIGNED modIn(void);
int EatExtMessage(int uploading);
char *FindProtoName(int protocol);
void AddExternProtocolOptions(char **Opts, char upload);
int FindProtocolCode(int c, char upload);
char DoesNumerous(int protocol);
void UpProtsEnglish(char *target);
void DownProtsEnglish(char *target);

PROTOCOL *FindProtocolByName(char *name, char upload);

	/* These functions are located in LIBROOM.C */
void getRoom(int rm);
void putRoom(int rm);

	/* These functions are located in LIBTABL.C */
char readSysTab(char kill, char showMsg);
void *GetDynamic(unsigned size);

int writeSysTab(void);
int common_read(void *block, int size, int elements, FILE *fd, char showMsg);

void openFile(char *filename, FILE **fd);

	/* These functions are located in LIBLOG.C */
void getLog(logBuffer *lBuf, int n);
void putLog(logBuffer *lBuf, int n);

	/* These functions are located in LIBLOG2.C */
int PersonExists(char *name);

	/* These functions are located in LIBCRYP.C */
void crypte(void *buf, unsigned len, unsigned seed);
UNS_16 hash(char *str);

	/* These functions are located in LIBMSG.C */
char getMessage(int (*Source)(void), char FromNet, char all, char ClearOthers);

void InitMsgBase(void);
void InitBuffers(void);
void getMsgStr(int (*Source)(void), char *dest, int lim);
void startAt(FILE *whichmsg, struct mBuf *mFile, SECTOR_ID sect, int byt);
void unGetMsgChar(char c);
void ZeroMsgBuffer(MessageBuffer *msg);

int getMsgChar(void);

/* These functions are in libmsg also, but handle CC on msgs. */
void *ChkCC();
void DisplayCC();
/* void *EatCC(char *line);
void *MakeCC(char *name, char needstatic); */
void ShowCC(int where);

	/* These functions are located in LIBNET.C */
void getNet(int n, NetBuffer *buf);
void putNet(int n, NetBuffer *buf);

char normId(label source, label dest);

	/* These functions are located in ARCH.C */
void *EatNMapStr(char *line);
void *EatArchRec(char *line);
void *NtoStrInit(int num, char *str, int num2, char needstatic);
void *ChkNtoStr();
void *ChkStrForElement();
void *ChkStrtoN();
void WrtNtoStr();
void WrtArchRec();
void WrtCC(void *dd);
void FreeNtoStr();

char *AskForNSMap(SListBase *base, int val);
int  GetArchSize(int num);

	/* These functions are located in CALLLOG.C */
void logMessage(int typemessage, UNS_32 val, int flags);
void CallMsg(char *fn, char *str);
void fileMessage(char mode, char *fn, char IsDL, int protocol, long size);

#ifdef NET_INTERFACE

int searchNet(char *forId, NetBuffer *buf);
int searchNameNet(label name, NetBuffer *buf);
char NetValidate(char talk);
char netInfo(char GetName);
int netMessage(int uploading);
void DiscardMessage(char *name, char *filename);
char MakeNetted(int m, char tonet);
int FindRouteIndex(int system);
int getNetChar(void);
void prNetStyle(int NotMsgBase, int (*SourceFn)(void),
		int (*SendMethod)(int c), char GetMsg, char *TargetSystem);
int RoutePath(char *rp, char *str);
void NetPrivs(label who);
void netController(int NetStart, int NetLength, MULTI_NET_DATA whichNets,
						char mode, UNS_16 flags);
void netStuff(void);
void ParticipatingNodes(char *target);
int IsRoomRoutable(SharedRoomData *room, int system, int roomslot, void *d);
void CacheMessages(MULTI_NET_DATA whichNets, char VirtOnly);

#endif	/* NET_INTERFACE */

void NetInit(void);
void DelFile(DirEntry *f);
void killConnection(char *x);
void netResult(char *msg);

#ifdef NET_INTERNALS
	/* These functions are located in NETMISC.C */
int GetSystemName(char *buf, int curslot, MenuId id);
int GetSystemId(char *buf, int curslot, MenuId id);
char called_stabilize(void);
SystemCallRecord *NewCalledRecord(int slot);
char RecipientAvail(void);
char check_for_init(char mode);
SystemCallRecord *callOut(int i);
char DirectRoute(NetBuffer *system);
char SendPrepAsNormal(char *work, int *count);
char roomsShared(int slot);
char HasOutgoing(SharedRoom *room);
char AnyCallsNeeded(MULTI_NET_DATA whichNets);

int AckStabilize(int index);
int AddNetMsgs(char *base, void (*procFn)(), char zap, int roomNo, char AddNetArea);
int getNMsgChar(void);
int timeLeft(void);
int addSendFile(char *Files, int arg);
int addNetMem(char *netnum);
int MemberNets(char *netnum, int add);
int makeCall(char EchoErr, MenuId id);
int subNetMem(char *netnum);
int needToCall(int system, MULTI_NET_DATA CurrentNets);

void inRouteMail(void);
void KillTempFiles(int whichnode);
void NodeValues(MenuId id);
void sendWCFile(FILE *fd);
void inMail(void);
void ClearRoomSharing(void);
void initNetRooms(void);
void AreaCode(char *Id, char *Target);
void setTime(int NetStart, int NetLength);
void moPuts(char *s);
void writeNet(char idsAlso, char LocalOnly);
void getSendFiles(MenuId id, label sysName);
void addNetNode(void);
void editNode(void);
void fileRequest(void);
void RoomStuff(char *title);
int DumpRoom(SharedRoomData *room, int system, int roomslot, void *d);
void EvalNeed(int searcher, MULTI_NET_DATA whichNets);
void parseBadRes(char *c);      /* temporary until next major release */

	/* These functions are located in NETCACHE.C */
char SendFastTransfer(void);
char SendMapFile(void);
void ITL_Line(char *data);
char MapFileAccepted(void);
void netFastTran(struct cmd_data *cmds);
int ReceiveProtocolValidate(int proto, PROTOCOL **External);
void KillCacheFiles(int which);
void ReadFastFiles(char *dir);
void CacheSystem(int system, char moo);
void RecoverMassTransfer(char *line);

	/* These functions are located in NETCALL.C */
void caller_stabilize(void);
void checkMail(void);
void Addressing(int system, SharedRoom *room, char *commnd, char **send1,
			char **send2, char **send3, char **name, char *doit);
int  findAndSend(int commnd, SharedRoomData *room,
		int (*MsgSender)(SharedRoomData *r), label roomName,
		int (*MsgReceiver)(SharedRoomData *r, char y));

void NetCC();
void SetHighValues(SharedRoomData *room);
void WriteCC();
void readNegMail(char talk);
void roleReversal(char reversed, int interrupted);
void sendStuff(char reversed, char SureDoIt, char SendRooms);
void SendPwd(void);
void sendId(void);
void sendMail(void);
void sendSharedRooms(void);
void doSendFiles(void);
void askFiles(void);
void sendHangUp(void);
void no_good(char *str, char hup);
void netSendFile(DirEntry *fn);
void SendHostFile(char *fn);

ValidateShowMsg_f_t NetRoute;
char caller(void);
char sendNetCommand(struct cmd_data *cmds, char *error);
char multiReceive(struct fl_req *file_data);
char SetAddressFlags(char **s1, char **s2, char **s3, int *commnd, int rover);

int send_direct_mail(int which, char *name);
int RoomSend(SharedRoomData *room);
int RoomReceive(SharedRoomData *room, char ReplyFirst);

	/* These functions are located in NETRCV.C */
void system_called(void);
void CheckRecipient();
void rcvStuff(char reversed);
void RecoverNetwork(void);
void netPwd(struct cmd_data *cmds);
void doResults(void);
void ReadNetRoomFile(SharedRoom *room, char *fn);
void UpdateRecoveryFile(char *val);
void getId(void);
void getNextCommand(struct cmd_data *cmds);
void grabCommand(struct cmd_data *cmds, char *sect);
void reply(char state, char *reason);
void reqReversal(struct cmd_data *cmds, char reversed);
void reqCheckMail(void);
void CheckRecipient(char *d);
void targetCheck(void);
void doNetRooms(void);
void getMail(void);
void reqSendFile(struct cmd_data *cmds);
void netFileReq(struct cmd_data *cmds);
void netRRReq(struct cmd_data *cmds, char SendBack);
void netRoomReq(struct cmd_data *cmds);
void recNetMessages(SharedRoom *room, char *name, int slot, char ReplyFirst);

void netMultiSend(DirEntry *fn);
char RoomRoutable(RoomSearch *data);

	/* These functions are located in VIRT.C */
int VirtRoomRoutable(SharedRoomData *room, int system, int roomslot, void *d);
int SendVirtual(SharedRoomData *room);
int RecVirtualRoom(SharedRoomData *room, char ReplyFirst);
void SetUpForVirtuals(SharedRoom *room, int *roomslot, char *fn);
int SendVirtualRoom(SharedRoomData *room, int system, int roomslot, void *d);
int DumpVRoom(SharedRoomData *room, int system, int roomslot, void *d);
int UnCacheVirtualRoom(SharedRoomData *room, int system, int which, void *d);
int VRNeedCall(SharedRoomData *room, int system, int roomslot, char *d);
int VirtualRoomOutgoing(SharedRoomData *room, int system, int roomslot,
								char *d);
int VirtualRoomMap(SharedRoomData *room, int system, int roomslot, void *d);
int CacheVirtualRoom(SharedRoomData *room, int sys, int roomslot, void *d);
int FindVirtualRoom(char *name);

void VirtInit(void);
void InitVNode(int slot);
void UpdVirtStuff(char);
void VirtSummary(char);

	/* These functions reside in NETITL.C */
char ITL_Receive(char *FileName, char ReplyFirst, char OpenIt, 
				int (*W)(int c), int (*CloseFn)(FILE *f));
char ITL_Send(char mode);
char ITL_SendMessages(void);
char ITL_StartRecMsgs(char *FileNm, char ReplyFirst, char OpenIt,
						int (*OverRide)(int c));

int increment(int c);

void ITL_RecCompact(struct cmd_data *cmds);
void ITL_InitCall(void);
void ITL_StopSendMessages(void);
void ITL_DeInit(void);
void ITL_optimize(char both);
void ITL_rec_optimize(struct cmd_data *cmds);

#endif	/* NET_INTERNALS */
int sendITLchar(int c);

char ReqNodeName(char *prompt, label target, label domain, int flags,
						NetBuffer *nBuf);

	/* These functions reside in HOT_HELP.C, 
	courtesy Paul Gauthier. */
int printHelp(char *fileName, int flags);


	/* These functions are located in VORTEX.C */
void ClearVortex(void);
void FinVortexing(void);
void InitVortexing(void);
void VortexInit(void);

char NotVortex(void);

long ReadTime(char *time);

	/* These functions are located in ROUTE.C */
void prStStyle(int mode, int (*SourceFn)(void), char *Name,
					int (*M)(int c), char *Domain);
char AnyRouted(int i);
int RoutingNode(int slot);
char RouteHere(char *Id, char *Name, char *Domain);
char AcceptRoute(char *id, char *name);
char FindTheNode(char *id, char *name);

int ReadRoutedDest(int c);
int FindRouteSlot(void);
int SendRoutedAsLocal(void);
int IdStName(char *name);
int ReadRouted(void);

void RouteOut(NetBuffer *nBuf, int node, char DirectConnect);
void netRouteMail(struct cmd_data *cmds);
void NextRouteName(SYS_FILE fn);
void AdjustRoute(void);
void MakeRouted(void);

char *UseNetAlias(char *Name, char FindAlias);
char SendRouteMail(char *filename, char *domainname, char *Tid, char *Tname,
				char LocalCheck);

	/* Compact.C */
int StartEncode(int (*func)(int c));
int StartDecode(int (*func)(int c));
int Encode(int c);
int Decode(int c);

void StopDecode(void);
void StopEncode( void );

	/* Mailfwd.c */
void OpenForwarding(void);
void AddMailForward(char *acct, char *system, char *fwdacct);
void KillLocalFwd(char *name);

char *FindLocalForward(char *name);
char CheckForSpecial(int second, int third);

	/* Stroll.C */
void StrollIt(void);

	/* BIO.C */
void EditBio(void);
char GetBioInfo(int which);
void SaveBioInfo(int which);
void ClearBio(int which);
void BioDirectory(void);

	/* SHARED.C */
int KillShared(SharedRoomData *roo, int syste, int roomslo, void *d);
void KillSharedRoom(int room);
void initSharedRooms( char check );
SharedRoomData *NewSharedRoom(void);
void EachSharedRoom(int system,
    int (*func)(SharedRoomData *room, int system, int roomslot, void *d),
    int (*virtfunc)(SharedRoomData *room, int system, int roomslot, void *d),
    void *data);
void EachSharingNode(int room, int flags,
    int (*func)(SharedRoomData *room, int system, int roomslot, void *d),
    void *data);
void UpdateSharedRooms(void);

	/* OFFLINE.C */
void OR_Upload(OfflineReader *Reader, char Protocol);
void OffLineInit(void);
void AddOfflineReaderOptions(char **Opts, char upload);
int DropFile(int dir, char *tempdir);
ValidateShowMsg_f_t ORWriteMsg;
int OR_Result(char *filename);
void OfflineUp(char *target);
void OfflineDown(char *target);

	/* MAIL.C */
void InitIgnoreMail(void);
int IgnoreThisUser(int slot);
int AcceptableMail(int from, int target);
int IgnoredUsers(int from, int (*fn)(int));
int IgMailRemoveEntries(int source, int target);
void IgMailCleanup(void);

#else
	/*
	 * Else, we need these in place.
	 * NOTE: This file has not been compiled with ANSI_PROTOTYPING
	 * not defined!
	 */

char *GetDynamic(),
     *formRoom(),
     *formDate(),
     *ChkPreempt(),
     *lbyte(),
     *matchString(),
     *formatSummary(),
     *findArchiveName(),
     *formHeader(),
     *findListName();

	/*
	 * "standard functions"
	 */
char *fgets(),
     *strchr(),
     *strcat();

char iChar(),
     BBSCharReady(),
     getCommand(),
     getYesNo(),
     mAbort(),
     getMod(),
     coreGetYesNo(),
     dGetWord(),
     normId(),
     getNetMessage(),
     callOut(),
     putMessage(),
     conGetYesNo();

long getNumber(),
     WeekDiff();

int  roomExists(),
     Xtime(),
     Ytime(),
     eventSort(),
     increment(),
     partialExist(),
     ShowZF(),
     sortDir(),
     inMail(),
     targetCheck(),
     netMultiSend(),
     RoomSend(),
     RoomReceive(),
     mPrintf(),
     getNetChar();

	/*
	 * More "standard functions"
	 */
int  printf();

	/* 
	 * These are used (mostly) with the wildCard() function
	 */
int  fDir(),
     ARCdir(),
     putFLChar(),
     putBufChar(),
     getSize(),
     netSendFile(),
     ShowVerbose();

	/*
	 * These are used with the floorRunner() function.
	 */
int  NSRoomHasNew(),
     SkippedNewRoom(),
     Zroom(),
     FindAny(),
     FSroom(),
     MoveToFloor(),
     MaimOrKill(),
     DispRoom(),
     RoomHasNew(),
     CheckFloor();

	/*
	 * These are used for getList() calls
	 */
int  makeKnown(),
     makeUnknown(),
     knownHosts(),
     killFromList(),
     addNetMem(),
     subNetMem(),
     IntStr();

unsigned int modIn();

void *realloc();	/* This may not be necessary. */

#ifdef CONFIGURE

MSG_NUMBER findHighestNative();

int msgSort(), logSort();

char *getLVal();

MULTI_NET_DATA FigureNets();

#endif

#endif
