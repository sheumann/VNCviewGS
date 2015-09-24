/********************************************************************
* vncview.cc - main program code for VNCview GS
*******************************************************************/

#if __ORCAC__
#pragma lint -1
#endif

#if DEBUG
/* #pragma debug 25 */
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <orca.h>
#include <Locator.h>
#include <MiscTool.h>
#include <Event.h>
#include <Menu.h>
#include <QuickDraw.h>
#include <QDAux.h>
#include <Window.h>
#include <Desk.h>
#include <Resources.h>
#include <Memory.h>
#include <Control.h>
#include <LineEdit.h>
#include <TCPIP.h>
#include <Scrap.h>
#include "vncview.h"
#include "VNCsession.h"
#include "vncdisplay.h"
#include "menus.h"
#include "colortables.h"
#include "mouse.h"
#include "keyboard.h"
#include "clipboard.h"

#define noMarinettiError        2001
#define outOfMemoryError        2002

#define disconnectTCPIPAlert    2003

#define NCWindow        1000    /* Offset for "New Connection"  */
                                /* window and its controls  */
#define winNewConnection    1
#define btnConnect          1
#define btnCancel           2
#define linServer           3
#define txtServer           4
#define txtServerInfo       5
#define txtPassword         6
#define linPassword         7
#define txtDisplay          8
#define rectDisplay         9
#define txtColor            10
#define txtGray             11
#define rad320              12
#define rad640              13
#define chkShared           16
#define chkClipboard        17
#define txtTransfers        23
#define chkEmul3Btn         18
#define chkViewOnly         19
#define txtPreferredEncoding    24
#define radRaw              25
#define radHextile          26


BOOLEAN done = FALSE;           /* are we done, yet? */
EventRecord myEvent;            /* event record for menu mode */
GrafPortPtr newConnWindow;      /* pointer to new connection window */
BOOLEAN vncConnected = FALSE;   /* are we connected to a VNC host */
int menuOffset;                 /* Indicates which menu bar is active */
Ref startStopParm;              /* tool start/shutdown parameter */
BOOLEAN colorTablesComplete = FALSE;    /* Are the big color tables complete */


/* Connection options */
int hRez = 320;
BOOLEAN requestSharedSession = TRUE;
BOOLEAN allowClipboardTransfers = TRUE;
BOOLEAN emulate3ButtonMouse = TRUE;
BOOLEAN viewOnlyMode = FALSE;
BOOLEAN useHextile = FALSE;
char vncServer[257];
char vncPassword[10];


/***************************************************************
* DrawContents - Draw the contents of the active port
***************************************************************/

#pragma databank 1

void DrawContents (void) {
    PenNormal();                    /* use a "normal" pen */
    DrawControls(GetPort());        /* draw controls in window */
}

#pragma databank 0

/***************************************************************
* DoAbout - Draw our about box
***************************************************************/

void DoAbout (void) {
    #define alertID 1                       /* alert string resource ID */

    AlertWindow(awCString+awResource, NULL, alertID);

    #undef alertID
}

/***************************************************************
* DoNewConnection - Show the New Connection window
***************************************************************/

void DoNewConnection (void) {
    unsigned int masterSCB;
    
    masterSCB = GetMasterSCB();
    MakeThisCtlTarget(GetCtlHandleFromID(newConnWindow, linServer));
    ShowWindow(newConnWindow);
    SelectWindow(newConnWindow);
}

/***************************************************************
* DoClose - Close the frontmost window/connection
* Parameters:
*   wPtr - window to close
***************************************************************/

void DoClose (GrafPortPtr wPtr) {
    if (wPtr == newConnWindow) {
        HideWindow(wPtr);
    }
    else if (wPtr && vncConnected) {    /* Close VNC session window */
        CloseWindow(wPtr);
        CloseTCPConnection();
        vncConnected = FALSE;
        EnableMItem(fileNewConnection);
        InitMenus(0);        
        myEvent.wmTaskMask = 0x001F79FF; /* let TaskMaster handle keys again */
        if (cursor) {
            InitCursor();
            free(cursor);
            cursor = NULL;
        }
    };
}

/***************************************************************
* DoLEEdit - Handle edit menu items for LineEdit controls
* Parameters:
*   editAction: Action selected from edit menu
***************************************************************/

void DoLEEdit (int editAction) {
    CtlRecHndl ctl;         /* target control handle */
    unsigned long id;       /* control ID */
    GrafPortPtr port;       /* caller's GrafPort */

    port = GetPort();
    SetPort(newConnWindow);
    ctl = FindTargetCtl();
    id = GetCtlID(ctl);
    if ((id == linServer) || (id == linPassword)) {
        LEFromScrap();
        switch (editAction) {
            case editCut:   if (id == linServer) {
                                LECut((LERecHndl) GetCtlTitle(ctl));
                            };              
                            LEToScrap();
                            break;
            case editCopy:  if (id == linServer) {
                                LECopy((LERecHndl) GetCtlTitle(ctl));
                            };              
                            LEToScrap();
                            break;
            case editPaste: LEPaste((LERecHndl) GetCtlTitle(ctl));  
                            break;
            case editClear: LEDelete((LERecHndl) GetCtlTitle(ctl));     
                            break;
        };
    };
    SetPort(port);
}

/***************************************************************
* HandleMenu - Initialize the menu bar.
***************************************************************/

void HandleMenu (void) {
    int menuNum, menuItemNum;               /* menu number & menu item number */

    menuNum = myEvent.wmTaskData >> 16;
    menuItemNum = myEvent.wmTaskData;
    switch (menuItemNum) {                  /* go handle the menu */
        case appleAbout:                DoAbout();                      break;

        case fileNewConnection:         DoNewConnection();              break;
        case fileClose:                 DoClose(FrontWindow());         break;
        case fileQuit:                  Quit();                         break;

        case editCut:                   DoLEEdit(editCut);              break;
        case editCopy:                  DoLEEdit(editCopy);             break;
        case editPaste:                 DoLEEdit(editPaste);            break;
        case editClear:                 DoLEEdit(editClear);            break;
        case editShowClipboard:         ShowClipboard(0x8000, 0);       break;
        case editSendClipboard:         DoSendClipboard();              break;
    }
    HiliteMenu(FALSE, menuNum);             /* unhighlight the menu */
}

/***************************************************************
* HandleControl - Handle a control press in the New Conn. window
***************************************************************/

void HandleControl (void) {
    switch (myEvent.wmTaskData4) {
        case btnConnect:    DoConnect();                                break;
        case btnCancel:     DoClose(newConnWindow);                     break;
        case txtColor:      SetCtlValueByID(TRUE, newConnWindow,
                                rad320);
                            /* Fall through */
        case rad320:        hRez = 320;             /* "320x200" */     break;
        case txtGray:       SetCtlValueByID(TRUE, newConnWindow,
                                rad640);
                            /* Fall through */
        case rad640:        hRez = 640;             /* "640x200" */     break;
        case chkShared:     requestSharedSession = !requestSharedSession;
                                                                        break;
        case chkClipboard:  allowClipboardTransfers = !allowClipboardTransfers;
                                                                        break;
        case chkEmul3Btn:   emulate3ButtonMouse = !emulate3ButtonMouse; break;
        case chkViewOnly:   viewOnlyMode = !viewOnlyMode;               break;
        case txtTransfers:  allowClipboardTransfers = !allowClipboardTransfers;
                            SetCtlValueByID(allowClipboardTransfers,
                                newConnWindow, chkClipboard);           break;
        case radRaw:        useHextile = FALSE;                         break;
        case radHextile:    useHextile = TRUE;                          break;
    };
}

/***************************************************************
* InitMenus - Initialize the menu bar.
***************************************************************/

void InitMenus (int offset) {
    #define menuID 1                        /* menu bar resource ID */

    int height;                             /* height of the largest menu */
    MenuBarRecHndl menuBarHand = 0;         /* for 'handling' the menu bar */
    MenuBarRecHndl oldMenuBarHand;
                                            /* create the menu bar */
    oldMenuBarHand = menuBarHand;
    menuBarHand = NewMenuBar2(refIsResource, menuID+offset, NULL);
    SetSysBar(menuBarHand);
    SetMenuBar(NULL);
    FixAppleMenu(1);                        /* add desk accessories */
    height = FixMenuBar();                  /* draw the completed menu bar */
    DrawMenuBar();
    if (oldMenuBarHand)
        DisposeHandle((Handle) oldMenuBarHand);

    menuOffset = offset;                    /* So we can tell which menu is active */

    #undef menuID
}

/***************************************************************
* CheckMenus - Check the menus to see if they should be dimmed
***************************************************************/

void CheckMenus (void) {
    GrafPortPtr activeWindow;       /* Front visible window */
    static GrafPortPtr lastActiveWindow;

    activeWindow = FrontWindow();

    /* Speed up common case (no change since last time) */
    if (activeWindow == lastActiveWindow)
        return;

    lastActiveWindow = activeWindow;

    if (activeWindow) {
        if (GetSysWFlag(activeWindow)) {        /* NDA window is active */
            EnableMItem(fileClose);
            EnableMItem(editUndo);
            EnableMItem(editCut);
            EnableMItem(editCopy);
            EnableMItem(editPaste);
            EnableMItem(editClear);
        }
        else if (activeWindow == newConnWindow) { /* New Connection window */
            EnableMItem(fileClose);
            DisableMItem(editUndo);
            EnableMItem(editCut);
            EnableMItem(editCopy);
            EnableMItem(editPaste);
            EnableMItem(editClear);
        }
        else if (activeWindow == vncWindow) {
            DisableMItem(editUndo);
            DisableMItem(editCopy);
            DisableMItem(editCut);
            DisableMItem(editPaste);
            DisableMItem(editClear);
        }
    }
    else {                      /* no editable window on top */
        DisableMItem(fileClose);
        DisableMItem(editUndo);
        DisableMItem(editCut);
        DisableMItem(editCopy);
        DisableMItem(editPaste);
        DisableMItem(editClear);
    };

    if (vncConnected) {             /* VNC connection present */
        DisableMItem(fileNewConnection);
        EnableMItem(fileClose);
        if (viewOnlyMode)
            DisableMItem(editSendClipboard);
        else
            EnableMItem(editSendClipboard);
    }
    else {
        DisableMItem(editSendClipboard);
    }
}

/* InitScreen - Set up color tables and SCBs to appropriate values
 */
void InitScreen (void) {
    static ColorTable gray640Colors = {
        0x0000, 0x0555, 0x0AAA, 0x0FFF, 0x0000, 0x0555, 0x0AAA, 0x0FFF,
        0x0000, 0x0555, 0x0AAA, 0x0FFF, 0x0000, 0x0555, 0x0AAA, 0x0FFF
    };  

        /* Apple menu uses color tables 1 through 6 */
        SetColorTable(7, &gray640Colors);
        SetAllSCBs(0x87);                   /* 640 mode with gray640Colors */
        InitPalette();                      /* Restore Apple Menu colors */
}

void Quit (void) {
    /* Done with event loop - now quitting */
    if (vncConnected)                   /* Disconnect if still connected */
        CloseTCPConnection();

    if (readBufferHndl)
        DisposeHandle(readBufferHndl);  /* Get rid of TCPIP read buffer hndl */

    if (bigcoltab320)
        free(bigcoltab320);
    if (bigcoltab640a)
        free(bigcoltab640a);
    if (bigcoltab640b)
        free(bigcoltab640b);
    if (cursor)
        free(cursor);

    /* Ask the user if we should disconnect only if the connection */
    /* is not "permanent," i.e. started when the system boots up.  */
    if (TCPIPGetConnectStatus() && (!TCPIPGetBootConnectFlag()))
        if (AlertWindow(awResource+awButtonLayout, NULL, disconnectTCPIPAlert))
        {
            WaitCursor();
            /* Must use force flag below because Marinetti will still count
             * our ipid as logged in (proventing non-forced disconnect)
             * for several minutes after the TCPIPLogout call. */
            TCPIPDisconnect(TRUE, &DisplayConnectStatus);
            if (connectStatusWindowPtr != NULL)
                CloseWindow(connectStatusWindowPtr);
        }

    UnloadScrap();                          /* Save scrap to disk */

    TCPIPShutDown();                        /* Shut down Marinetti */
    UnloadOneTool(54);
    ShutDownTools(1, startStopParm);        /* shut down the tools */
    exit(0);
}

/***************************************************************
* Main - Initial startup function
***************************************************************/

int main (void) {
    int event;                          /* event type returned by TaskMaster */

    #define wrNum 1001                  /* New Conn. window resource number */

    startStopParm =                     /* start up the tools */
        StartUpTools(userid(), 2, 1);
    if (toolerror() != 0) {
        GrafOff();
        SysFailMgr(toolerror(), "\pCould not start tools: ");
    }

    readBufferHndl = NewHandle(1, userid(), 0, NULL);
    
    LoadOneTool(54, 0x200);             /* load Marinetti 2.0+ */
    if (toolerror()) {                  /* Check that Marinetti is available */
        SysBeep();                      /* Can't load Marinetti.. */
        InitCursor();                   /* Activate pointer cursor */
        AlertWindow(awResource, NULL, noMarinettiError);
        Quit();                         /* Can't proceed, so just quit */
    }
    else                                /* Marinetti loaded successfully */
        TCPIPStartUp();                 /* ... so activate it now */

    if (toolerror()) {                  /* Get handle for TCPIP read buffer */
        SysBeep();
        InitCursor();
        AlertWindow(awResource, NULL, outOfMemoryError);
        Quit();
    }

    if (!AllocateBigColorTables()) {
        SysBeep();
        InitCursor();
        AlertWindow(awResource, NULL, outOfMemoryError);
        Quit();
    }

    InitScreen();                       /* Set up color tables */

    LoadScrap();                        /* put scrap in memory */
    InitMenus(0);                       /* set up the menu bar */
    InitCursor();                       /* start the arrow cursor */

    vncConnected = FALSE;               /* Initially not connected */

    newConnWindow = NewWindow2("\p  New VNC Connection  ", 0,
        DrawContents, NULL, 0x02, wrNum, rWindParam1);
    #undef wrNum

    DoNewConnection();                  /* Display new connection window */

                                        /* main event loop */
    myEvent.wmTaskMask = 0x001F79FF;  /* let TaskMaster do everything needed */
    while (!done) {
        CheckMenus();
        event = TaskMaster(everyEvent, &myEvent);
        if (vncConnected)
            SendModifiers();
        switch (event) {
            case wInSpecial:
            case wInMenuBar:    HandleMenu();
                                break;
            case wInGoAway:     DoClose((GrafPortPtr) myEvent.wmTaskData);
                                break;
            case wInControl:    HandleControl();
                                break;
            case wInContent:    if (vncWindow && ((GrafPortPtr)
                                            myEvent.wmTaskData == vncWindow))
                                    DoPointerEvent();
                                break;
            case nullEvt:       if (vncConnected) DoPointerEvent();
                                break;
            case keyDownEvt:
            case autoKeyEvt:    ProcessKeyEvent();
        }
        if (vncConnected)
            ConnectedEventLoop();
        else if (colorTablesComplete == FALSE)
            if (MakeBigColorTables(256))
                colorTablesComplete = TRUE;
    }

        Quit();
}
