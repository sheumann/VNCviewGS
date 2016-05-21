#if __ORCAC__
#pragma lint -1
#pragma noroot
segment "VNCview GS";
#endif

#include <window.h>
#include <quickdraw.h>
#include <qdaux.h>
#include <desk.h>
#include <memory.h>
#include <resources.h>
#include <tcpip.h>
#include <menu.h>
#include <control.h>
#include <misctool.h>
#include <scrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <event.h>
#include <limits.h>
#include <orca.h>

#include "vncsession.h"
#include "vncview.h"
#include "vncdisplay.h"
#include "colortables.h"
#include "menus.h"
#include "clipboard.h"
#include "desktopsize.h"
#include "mouse.h"
#include "keyboard.h"
#include "copyrect.h"
#include "raw.h"
#include "hextile.h"

unsigned int fbHeight;
unsigned int fbWidth;

BOOLEAN displayInProgress;

static unsigned int numRects;
unsigned int rectX;
unsigned int rectY;
unsigned int rectWidth;
unsigned int rectHeight;
static unsigned long rectEncoding;

GrafPortPtr vncWindow;

/* VNC session window dimensions */
unsigned int winHeight;
unsigned int winWidth;

/* On the next 2 structs, only certain values are permanently zero.
 * Others are changed later.
 */
struct LocInfo srcLocInfo = {0, 0, 0, {0, 0, 0, 0} };

/* Used by multiple encodings */
Rect srcRect = {0, 0, 0, 0};
unsigned char *pixTransTbl;

BOOLEAN checkBounds = FALSE;    /* Adjust drawing to stay in bounds */

#define txtColor        10
#define txtGray         11
#define txtTransfers    23

/* Send a request to be sent the data to redraw the window when part of it has
 * been erased.  It will be a while before the data is fully redrawn, but this
 * is an unavoidable penalty of opening other windows over the main VNC window.
 */
#pragma databank 1
static void VNCRedraw (void) {
    RegionHndl updateRgnHndl;

    updateRgnHndl = vncWindow->visRgn;

    SendFBUpdateRequest(FALSE,
                (**updateRgnHndl).rgnBBox.h1,
                (**updateRgnHndl).rgnBBox.v1,
                (**updateRgnHndl).rgnBBox.h2 - (**updateRgnHndl).rgnBBox.h1,
                (**updateRgnHndl).rgnBBox.v2 - (**updateRgnHndl).rgnBBox.v1);

    checkBounds = TRUE;
}
#pragma databank 0

/* Change Super Hi-Res display to the specified resolution (640 or 320).
 * Uses the procedure described in IIgs Tech Note #4.
 */
static void ChangeResolution(int rez) {
    static Handle dpSpace;
    unsigned int masterSCB;

    hRez = rez;

    winHeight = WIN_HEIGHT;
    winWidth = (rez == 640) ? WIN_WIDTH_640 : WIN_WIDTH_320;

    /* Set up pixel translation table for correct graphics mode */
    if (rez == 320)
        pixTransTbl = coltab320;
    else                            /* 640 mode */
        pixTransTbl = coltab640;

    srcLocInfo.portSCB = (rez == 640) ? 0x87 : 0x00;

    /* Check if we need to change modes */
    masterSCB = GetMasterSCB();
    if (        ( (masterSCB & 0x80) && (rez == 640)) ||
                (!(masterSCB & 0x80) && (rez == 320)) ) {
        return; /* Already in right mode, so don't change things */
    }

    /* Perform the basic procedure described in IIgs TN #4 */
    CloseAllNDAs();
    QDAuxShutDown();
    QDShutDown();
    if (dpSpace == NULL)
        dpSpace = NewHandle(0x0300, userid(),
                        attrLocked|attrFixed|attrNoCross|attrBank, 0x00000000);
    QDStartUp((Word) *dpSpace, (rez == 640) ? 0xC087 : 0xC000, 0, userid());
                    /* SCB 0x87 gives 640 mode with our custom gray palette */
    GrafOff();
    QDAuxStartUp();
    ClampMouse(0, (rez == 640) ? 639 : 319, 0, 199);
    HomeMouse();
    ShowCursor();
    WindNewRes();
    InitPalette();      /* Set up Apple menu colors before it is redrawn */
    MenuNewRes();
    CtlNewRes();
    RefreshDesktop(NULL);
    GrafOn();

    /* Position new connection window at default location for new mode */
    if (rez == 320) {                                                  
        MoveControl(25, 64, GetCtlHandleFromID(newConnWindow, txtColor));
        MoveControl(25, 87, GetCtlHandleFromID(newConnWindow, txtGray));
        MoveControl(134, 91, GetCtlHandleFromID(newConnWindow, txtTransfers));
        MoveWindow(2, 42, newConnWindow);
    }
    else {
        MoveControl(35, 64, GetCtlHandleFromID(newConnWindow, txtColor));
        MoveControl(35, 87, GetCtlHandleFromID(newConnWindow, txtGray));
        MoveControl(144, 91, GetCtlHandleFromID(newConnWindow, txtTransfers));
        MoveWindow(162, 42, newConnWindow);
    }
}

/* Display the VNC session window, first changing to the appropriate
 * resolution (as specified by the user) if necessary.
 */
void InitVNCWindow(void) {
#define wrNum640 1003
#define wrNum320 1004
    BOOLEAN resize = FALSE;

    ChangeResolution(hRez);
    
    vncWindow = NewWindow2(NULL, 0, NULL, NULL, 0x02,
                                (hRez == 640) ? wrNum640 : wrNum320,
                                rWindParam1);

    if (fbWidth < winWidth) {
        winWidth = fbWidth;
        resize = TRUE;
    }
    if (fbHeight < winHeight) {
        winHeight = fbHeight;
        resize = TRUE;
    }
    if (resize)
        SizeWindow(winWidth, winHeight, vncWindow);

    SetContentDraw(VNCRedraw, vncWindow);

    SetDataSize(fbWidth, fbHeight, vncWindow);

    DrawControls(vncWindow);

    /* We also take the opportunity here to initialize the rectangle info. */
    numRects = 0;
    displayInProgress = FALSE;

#undef wrNum320
#undef wrNum640
}

/* Send a request to the VNC server to update the information for a portion of
 * the frame buffer.
 */
void SendFBUpdateRequest (BOOLEAN incremental, unsigned int x, unsigned int y,
                            unsigned int width, unsigned int height) {
    
    struct FBUpdateRequest {
            unsigned char messageType;
            unsigned char incremental;
            unsigned int  x;
            unsigned int  y;
            unsigned int  width;
            unsigned int  height;
    } fbUpdateRequest = {3 /* Message type 3 */};

    fbUpdateRequest.incremental = !!incremental;
    fbUpdateRequest.x = SwapBytes2(x);
    fbUpdateRequest.y = SwapBytes2(y);
    fbUpdateRequest.width = SwapBytes2(width);
    fbUpdateRequest.height = SwapBytes2(height);

    TCPIPWriteTCP(hostIpid, &fbUpdateRequest.messageType,
                    sizeof(fbUpdateRequest), TRUE, FALSE);
    /* No error checking here -- Can't respond to one usefully. */
}


/* Start responding to a FramebufferUpdate from the server
 */
static void DoFBUpdate (void) {
    unsigned int *dataPtr;          /* Pointer to header data */

    if (!DoWaitingReadTCP(15)) {
        DoClose(vncWindow);
        //printf("Closing in DoFBUpdate\n");
        return;
    }
    HLock(readBufferHndl);
    dataPtr = (unsigned int *) (((char *) (*readBufferHndl)) + 1);
    numRects = SwapBytes2(dataPtr[0]);                  /* Get data */
    rectX = SwapBytes2(dataPtr[1]);
    rectY = SwapBytes2(dataPtr[2]);
    rectWidth  = SwapBytes2(dataPtr[3]);
    rectHeight = SwapBytes2(dataPtr[4]);
    rectEncoding = SwapBytes4(*(unsigned long *)(dataPtr + 5));
    HUnlock(readBufferHndl);
}

/* The server should never send a color map, since we don't use a mapped
 * representation for pixel values.  If a malfunctioning server tries to
 * send us one, though, we read and ignore it.  This procedure could lock
 * up the system for several seconds or longer while reading data, which
 * would be bad but for the fact that it will never be called if the server
 * is actually working correctly.
 */
static void DoSetColourMapEntries (void) {
    unsigned int numColors;

    DoWaitingReadTCP(3);
    DoWaitingReadTCP(2);
    HLock(readBufferHndl);
    numColors = SwapBytes2((unsigned int) **readBufferHndl);
    HUnlock(readBufferHndl);
    for (; numColors > 0; numColors--) {
        DoWaitingReadTCP(6);
    }
}


/* Here when we're done processing one rectangle and ready to start the next.
 * If last FramebufferUpdate had multiple rectangles, we set up for next one.
 * If no more rectangles are available, we send a FramebufferUpdateRequest.
 */
void NextRect (void) {
    unsigned int *dataPtr;

    numRects--;
    if (numRects) {                 /* Process next rectangle */
        if (!DoWaitingReadTCP(12)) {
            //printf("Closing in NextRect\n");
            DoClose(vncWindow);
            return;
        }
        HLock(readBufferHndl);
        dataPtr = (unsigned int *) ((char *) (*readBufferHndl));
        rectX = SwapBytes2(dataPtr[0]);
        rectY = SwapBytes2(dataPtr[1]);
        rectWidth  = SwapBytes2(dataPtr[2]);
        rectHeight = SwapBytes2(dataPtr[3]);
        rectEncoding = SwapBytes4(*(unsigned long *)(dataPtr + 4));
        HUnlock(readBufferHndl);
        //printf("New Rect: X = %u, Y = %u, Width = %u, Height = %u\n", rectX, rectY, rectWidth, rectHeight);
    }
    else {                          /* No more rectangles from last update */
        unsigned long contentOrigin;
        Point * contentOriginPtr = (void *) &contentOrigin;

        contentOrigin = GetContentOrigin(vncWindow);
        SendFBUpdateRequest(TRUE, contentOriginPtr->h, contentOriginPtr->v,
                            winWidth, winHeight);
    }
}                         
    
void ConnectedEventLoop (void) {
    unsigned char messageType;
#define FBUpdate            0
#define SetColourMapEntries 1
#define Bell                2
#define ServerCutText       3

    if (FrontWindow() != vncWindow && menuOffset == noKB)
        InitMenus(0);
    else if (FrontWindow() == vncWindow && menuOffset != noKB)
        InitMenus(noKB);

    if (displayInProgress) {
        switch (rectEncoding) {
            case encodingRaw:   RawDraw();
                                return;
            case encodingHextile: HexDispatch();
                                return;
            default:            DoClose(vncWindow);
                                return;
        }
    }
    else if (numRects) {
        switch (rectEncoding) {
            case encodingHextile:
                                DoHextileRect();
                                return;
            case encodingRaw:   DoRawRect();
                                return;
            case encodingCopyRect:
                                DoCopyRect();
                                return;
            case encodingDesktopSize:
                                DoDesktopSize();
                                return;
            case encodingCursor:
                                DoCursor();
                                return;
            default:            DisplayConnectStatus (
                                    "\pInvalid rectangle from server.", FALSE);
                                DoClose(vncWindow);
                                //printf("Closing due to bad rectangle encoding %lu\n", rectEncoding);
                                //printf("rectX = %u, rectY = %u, rectWidth = %u, rectHeight = %u\n", rectX, rectY, rectWidth, rectHeight);
                                return;
        }
    }
    else if (DoReadTCP(1)) {            /* Read message type byte */
        HLock(readBufferHndl);
        messageType = ((unsigned char) **readBufferHndl);
        HUnlock(readBufferHndl);
        switch (messageType) {
            case FBUpdate:              DoFBUpdate();
                                        break;
            case SetColourMapEntries:   DoSetColourMapEntries();
                                        break;
            case Bell:                  SysBeep();
                                        break;
            case ServerCutText:         DoServerCutText();
                                        break;
            default:                    DisplayConnectStatus (
                                            "\pInvalid message from server.",
                                            FALSE);
                                        DoClose(vncWindow);
                                        return;
        }
    }
}
