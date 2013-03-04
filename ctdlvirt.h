/*
 *				ctdlvirt.h
 *
 * Virtual room data structures.
 */

/*
 *				History
 *
 * ??????? HAW	Created.
 */

#define LD_DIR          "backbone"
#define LOCAL_DIR       "peon"

#define LD_CHANGE       0x01    /* Bit settings */
#define LOC_CHANGE      0x02
#define SENT_DATA       0x04

typedef struct {
    label       vrName;         /* Name of the fiendish entity */
                        /* If this one not in use, make strLen(vrName) == 0 */
    MSG_NUMBER  vrHiLocal,      /* # of current highest from local msg-file */
                vrHiLD,         /* # of current highest from LD msg-file */
                vrLoLocal,      /* # of current lowest from local msg-file */
                vrLoLD;         /* # of current lowest from LD msg-file */
   char         vrChanged;      /* temporary for changes */
} VirtualRoom;

#define VRoomInuse(x)           strLen(VRoomTab[x].vrName)
#define VRoomKill(x)		zero_struct(VRoomTab[x])

#define VGetMode(x)	((x) & 7)
#define VSetMode(x, y)	x = (x & (~7)) + y;
#define VGetFA(x)	((x) & 8)
#define VSetFA(x)	x |= 8;
#define VUnSetFA(x)	x &= (~8);

#define NORMAL          0
#define BATCH           1

#ifdef V_ADMIN

typedef struct {
    char uses, mode;
    MSG_NUMBER WhenNormal;
    SharedRoomData *room;
} TempData;

#ifdef ANSI_PROTOTYPING

TempData *SetNtoVList(int roomNo, int VNo);

void ConNorVa(void);
void Rename(void);
void GenInit(void);
void UpdVirtStuff(void);
void AnalyzeArguments(int argc, char **argv);
void InteractMode(void);
void BatchMode(void);
void AddRoom(void);
void InitVirtual(void);
void Delete(void);
void Modify(void);
void Display(void);
void ResetNodes(int slot);
void GetInputList(int index, char *prompt, int (*func)());

int RemoveNodes(int index, label name);
int AddNodes(int index, label name);
int VirtualExists(label name);
int DoesShare(int NodeNo, int RoomNo);
int NorToVirtual(int VirtNo, TempData *list);
int ChgNodes(int index, label name);

char ReadMessage(int rover);
char GetConString(char *prompt, char *buffer, int length);

void ChkVirtArea(void);
void VADebug(void);
void SetVirtAreas(int RoomNo);
void KillVirtAreas(int RoomNo);
void splitIt(char *format, ...);

int NEUtilGetch(void);
int UtilGetch(void);
int Add2Room(label NewName);

#endif

#endif
