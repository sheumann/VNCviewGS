/********************************************************************
* vncview.h - functions not directly related to VNC session
********************************************************************/

#include <types.h>

extern GrafPortPtr newConnWindow;

extern int menuOffset;

/* Connection options */
extern int hRez;
extern BOOLEAN requestSharedSession;
extern BOOLEAN allowClipboardTransfers;
extern BOOLEAN emulate3ButtonMouse;
extern BOOLEAN viewOnlyMode;
extern BOOLEAN useHextile;
extern char vncServer[257];
extern char vncPassword[10];

extern EventRecord myEvent;             /* Event Record for TaskMaster */
extern BOOLEAN vncConnected;            /* Is the GS desktop active */
extern BOOLEAN colorTablesComplete;     /* Are the color tables complete */

extern void DoClose (GrafPortPtr wPtr);
extern void InitMenus (int);
