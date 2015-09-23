/********************************************************************
* vncview.cc - main program code for VNCview GS
********************************************************************/

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
#include "VNCsession.h"

#define appleMenu     			1
#define fileMenu      			2
#define editMenu      			3

#define appleAbout    			257

#define fileNewConnection       260
#define fileReturnToVNCSession	261
#define fileClose     			255
#define fileQuit      			256

#define editUndo      			250
#define editCut       			251
#define editCopy        		252
#define editPaste       		253
#define editClear       		254
#define editSendClipboard		262

#define noMarinettiError		2001
#define outOfMemoryError		2002

#define disconnectTCPIPAlert	2003

#define NCWindow		1000	/* Offset for "New Connection" 	*/
								/* window and its controls	*/
#define winNewConnection	1
#define btnConnect			1
#define btnCancel			2
#define linServer			3
#define txtServer			4
#define txtServerInfo		5
#define txtPassword			6
#define linPassword			7
#define txtDisplay			8
#define rectDisplay			9
#define radColor			10
#define radGray				11
#define rad320				12
#define rad640				13
#define chkLocalPtr			24
#define txtPointer			25
#define chkShared			16
#define chkClipboard		17
#define txtTransfers		23
#define chkEmul3Btn			18
#define chkViewOnly			19
#define txtDeleteSends		20
#define radDelete			21
#define radBackspace		22

BOOLEAN done = FALSE;           /* are we done, yet? */
EventRecord myEvent;            /* event record for menu mode */
GrafPortPtr newConnWindow;		/* pointer to new connection window */
BOOLEAN vncConnected = FALSE;	/* are we connected to a VNC host */

/* Connection options */
BOOLEAN color = TRUE;
int hRez = 320;
BOOLEAN requestSharedSession = FALSE;
BOOLEAN allowClipboardTransfers = TRUE;
BOOLEAN emulate3ButtonMouse = FALSE;
BOOLEAN viewOnlyMode = FALSE;
BOOLEAN localPointer = FALSE;
unsigned long deleteKeysym = 0xff08;
char vncServer[257];
char vncPassword[10];


/***************************************************************
* DrawContents - Draw the contents of the active port
***************************************************************/

#pragma databank 1

void DrawContents (void) {
	PenNormal();                    /* use a "normal" pen */
	DrawControls(GetPort());		/* draw controls in window */
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
	MakeThisCtlTarget(GetCtlHandleFromID(newConnWindow, linServer));
	ShowWindow(newConnWindow);
	}

/***************************************************************
* DoClose - Close the frontmost window/connection
* Parameters:
*	wPtr - window to close
***************************************************************/

void DoClose (GrafPortPtr wPtr)	{
	if (wPtr == newConnWindow) {
		HideWindow(newConnWindow);
		}
	else if (wPtr && vncConnected) {	/* Close VNC session if no other */
	/*	DisconnectVNCSession(); */      /* are open on top of VNC window */
		};
	}

/***************************************************************
* DoLEEdit - Handle edit menu items for LineEdit controls
* Parameters:
*	editAction: Action selected from edit menu
***************************************************************/

void DoLEEdit (int editAction) {
	CtlRecHndl ctl;			/* target control handle */
	unsigned long id;		/* control ID */
	GrafPortPtr port;		/* caller's GrafPort */

	port = GetPort();
	SetPort(newConnWindow);
	ctl = FindTargetCtl();
	id = GetCtlID(ctl);
	if ((id == linServer) || (id == linPassword)) {
		LEFromScrap();
		switch (editAction) {
	        case editCut:	if (id == linServer) {
                				LECut((LERecHndl) GetCtlTitle(ctl));
								};				
                            LEToScrap();
                            break;
            case editCopy:	if (id == linServer) {
			                	LECopy((LERecHndl) GetCtlTitle(ctl));
                                };				
                            LEToScrap();
                            break;
			case editPaste:	LEPaste((LERecHndl) GetCtlTitle(ctl)); 	
                   			break;
            case editClear:	LEDelete((LERecHndl) GetCtlTitle(ctl));		
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
		case appleAbout:     			DoAbout();						break;

		case fileNewConnection:			DoNewConnection();				break;
		case fileReturnToVNCSession:									break;
        case fileClose:   				DoClose(FrontWindow());			break;
		case fileQuit:       			done = TRUE;					break;

       	case editCut:           		DoLEEdit(editCut);				break;
        case editCopy:          		DoLEEdit(editCopy);				break;
        case editPaste:         		DoLEEdit(editPaste);			break;
        case editClear:         		DoLEEdit(editClear);		  	break;
		}
	HiliteMenu(FALSE, menuNum);             /* unhighlight the menu */
	}

/***************************************************************
* HandleControl - Handle a control press in the New Conn. window
***************************************************************/

void HandleControl (void) {
	switch (myEvent.wmTaskData4) {
        case btnConnect:	DoConnect();								break;
        case btnCancel:		DoClose(newConnWindow);						break;
        case radColor:		color = TRUE;								break;
       	case radGray:		color = FALSE;								break;
       	case rad320:		hRez = 320;				/* "320x200" */		break;
       	case rad640:		hRez = 640; 			/* "640x200" */		break;
       	case chkShared:		requestSharedSession = !requestSharedSession;
        																break;
       	case chkClipboard:	allowClipboardTransfers = !allowClipboardTransfers;
        																break;
        case chkEmul3Btn:	emulate3ButtonMouse = !emulate3ButtonMouse;	break;
       	case chkViewOnly:	viewOnlyMode = !viewOnlyMode;				break;
       	case radDelete:		deleteKeysym = 0xffff;	/* delete -> del */	break;
       	case radBackspace:	deleteKeysym = 0xff08;	/* delete -> bs */	break;
       	case txtTransfers:	allowClipboardTransfers = !allowClipboardTransfers;
        					SetCtlValueByID(!allowClipboardTransfers,
								newConnWindow, 17);						break;
        case chkLocalPtr:	localPointer = !localPointer;				break;
        case txtPointer:	SetCtlValueByID(!localPointer, newConnWindow, 24);
        					localPointer = !localPointer;	   			break;
        };
	}

/***************************************************************
* InitMenus - Initialize the menu bar.
***************************************************************/

void InitMenus (void) {
	#define menuID 1                        /* menu bar resource ID */

	int height;                             /* height of the largest menu */
	MenuBarRecHndl menuBarHand;             /* for 'handling' the menu bar */

	                                        /* create the menu bar */
	menuBarHand = NewMenuBar2(refIsResource, menuID, NULL);
	SetSysBar(menuBarHand);
	SetMenuBar(NULL);
	FixAppleMenu(1);                        /* add desk accessories */
	height = FixMenuBar();                  /* draw the completed menu bar */
	DrawMenuBar();

	#undef menuID
	}

/***************************************************************
* CheckMenus - Check the menus to see if they should be dimmed
***************************************************************/

void CheckMenus (void) {
	GrafPortPtr activeWindow;		/* Front visible window */

	activeWindow = FrontWindow();
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
        }
	else {						/* no editable window on top */
        DisableMItem(fileClose);
       	DisableMItem(editUndo);
       	DisableMItem(editCut);
       	DisableMItem(editCopy);
       	DisableMItem(editPaste);
       	DisableMItem(editClear);
		};

	if (vncConnected) {				/* VNC connection present */
     	EnableMItem(fileReturnToVNCSession);
       	EnableMItem(fileClose);
       	EnableMItem(editSendClipboard);
     	}
	else {
      	DisableMItem(fileReturnToVNCSession);
       	DisableMItem(editSendClipboard);
   		}
	}

/***************************************************************
* Main - Initial startup function
***************************************************************/

int main (void) {
	int event;                          /* event type returned by TaskMaster */
	Ref startStopParm;             	    /* tool start/shutdown parameter */

	#define wrNum 1001					/* New Conn. window resource number */

	startStopParm =                     /* start up the tools */
		StartUpTools(userid(), 2, 1);
	if (toolerror() != 0)
		SysFailMgr(toolerror(), "\pCould not start tools: ");
#if 0
	LoadOneTool(54, 0x200);				/* load Marinetti 2.0+ */
	if (toolerror()) {		    	    /* Check that Marinetti is available */
    	SysBeep();
		AlertWindow(awResource, NULL, noMarinettiError);
	    done = TRUE;
	    }
	else
		TCPIPStartUp();
#endif

	InitMenus();                        /* set up the menu bar */
	InitCursor();                       /* start the arrow cursor */

	vncConnected = FALSE;  				/* Initially not connected */

	newConnWindow = NewWindow2("\p  New VNC Connection  ", 0,
		DrawContents, NULL, 0x02, wrNum, rWindParam1);
	#undef wrNum

    DoNewConnection();					/* Display new connection window */

			              				/* main event loop */
	myEvent.wmTaskMask = 0x001F71FF;/* let TaskMaster do everything that's needed */
	while (!done) {
		CheckMenus();
        event = TaskMaster(everyEvent, &myEvent);
printf("In event loop after TaskMaster\n");
        }
	ShutDownTools(1, startStopParm);        /* shut down the tools */
	}
