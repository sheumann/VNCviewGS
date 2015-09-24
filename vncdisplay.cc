#include <window.h>
#include <quickdraw.h>
#include <qdaux.h>
#include <desk.h>
#include <memory.h>
#include "vncsession.h"
#include "vncview.h"
#include "vncdisplay.h"
#include "colortables.h"
#include "menus.h"
#include <resources.h>
#include <tcpip.h>
#include <menu.h>
#include <control.h>
#include <misctool.h>
#include <scrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <event.h>

unsigned int fbHeight;
unsigned int fbWidth;

BOOLEAN displayInProgress;
unsigned int drawingLine;		/* Line to be drawn while displaying */

unsigned int numRects;
unsigned int rectX;
unsigned int rectY;
unsigned int rectWidth;
unsigned int rectHeight;
unsigned long rectEncoding;

unsigned int hexXTiles, hexYTiles;	/* For in-process hextile processing */
unsigned int hexXTileNum, hexYTileNum;
unsigned int hexTileWidth, hexTileHeight;
unsigned char hexBackground, hexForeground;

BOOLEAN extraByteAdvance;

#define encodingRaw			0
#define encodingCopyRect	1
#define encodingRRE			2
#define encodingCoRRE		4
#define encodingHextile		5
#define encodingZRLE		16
#define encodingCursor		0xffffff11
#define encodingDesktopSize	0xffffff21

/* Used in Hextile encoding */
#define Raw					0x01
#define BackgroundSpecified	0x02
#define ForegroundSpecified	0x04
#define AnySubrects			0x08
#define SubrectsColoured	0x10
	
#define hexWaitingForSubencoding		1
#define hexWaitingForMoreInfo			2
#define hexWaitingForSubrect	 		4
#define hexWaitingForRawData			8

GrafPortPtr hexPort = NULL;

GrafPortPtr vncWindow;

/* VNC session window dimensions */
unsigned int winHeight;
unsigned int winWidth;

/* Data on state of raw rectangle drawing routines */
BOOLEAN checkBounds = FALSE;	/* Adjust drawing to stay in bounds */
unsigned int lineBytes;			/* Number of bytes in a line of GS pixels */
unsigned long pixels;

/* On the next 2 structs, only certain values are permanently zero.
 * Others are changed later.
 */
static struct LocInfo srcLocInfo = {0, 0, 0, {0, 0, 0, 0} };
static Rect srcRect = {0, 0, 0, 0};
unsigned char *destPtr;
unsigned char *pixTransTbl;

#define txtColor		10
#define txtGray			11
#define txtTransfers	23

/* Send a request to be sent the data to redraw the window when part of it has
 * been erased.  It will be a while before the data is fully redrawn, but this
 * is an unavoidable penalty of opening other windows over the main VNC window.
 */
#pragma databank 1
void VNCRedraw (void) {
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
void ChangeResolution(int rez) {
	static Handle dpSpace;
    unsigned int masterSCB;

    hRez = rez;

    winHeight = 174;
    winWidth = (rez == 640) ? 613 : 302;

    /* Set up pixel translation table for correct graphics mode */
    if (rez == 320)
	    pixTransTbl = coltab320;
    else							/* 640 mode */
	    pixTransTbl = coltab640;

    srcLocInfo.portSCB = (rez == 640) ? 0x87 : 0x00;

    /* Check if we need to change modes */
    masterSCB = GetMasterSCB();
    if (        ( (masterSCB & 0x80) && (rez == 640)) ||
			    (!(masterSCB & 0x80) && (rez == 320)) ) {
	    return;	/* Already in right mode, so don't change things */
        }

    /* Perform the basic procedure described in IIgs TN #4 */
	CloseAllNDAs();
    QDAuxShutDown();
    QDShutDown();
    if (dpSpace == NULL)
   		dpSpace = NewHandle(0x0300, userid(),
    	   				attrLocked|attrFixed|attrNoCross|attrBank, 0x00000000);
    QDStartUp((Word) *dpSpace, (rez == 640) ? 0x87 : 0x00, 0, userid());
					/* SCB 0x87 gives 640 mode with our custom gray palette */
    GrafOff();
    QDAuxStartUp();
    ClampMouse(0, (rez == 640) ? 639 : 319, 0, 199);
    HomeMouse();
    ShowCursor();
    WindNewRes();
    InitPalette();		/* Set up Apple menu colors before it is redrawn */
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

/* Send a KeyEvent message to the server
 */
void SendKeyEvent (BOOLEAN keyDownFlag, unsigned long key)
{
	struct KeyEvent {
	    unsigned char messageType;
        unsigned char keyDownFlag;
        unsigned int  padding;
        unsigned long key;
        } keyEvent = { 	4 /* Message Type 4 */,
        				0,
                        0  /* Zero the padding */
                     	};
                        
    keyEvent.keyDownFlag = !!keyDownFlag;
    keyEvent.key = SwapBytes4(key);
    TCPIPWriteTCP(hostIpid, &keyEvent.messageType, sizeof(keyEvent),
    				TRUE, FALSE);
	/* No error checking here -- Can't respond to one usefully. */	
}

/* Start responding to a FramebufferUpdate from the server
 */
void DoFBUpdate (void) {
	unsigned int *dataPtr;			/* Pointer to header data */

	if (!DoWaitingReadTCP(15)) {
        DoClose(vncWindow);
        //printf("Closing in DoFBUpdate\n");
        return;
        }
    HLock(readBufferHndl);
    dataPtr = (unsigned int *) (((char *) (*readBufferHndl)) + 1);
    numRects = SwapBytes2(dataPtr[0]);					/* Get data */
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
void DoSetColourMapEntries (void) {
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

/* Update the Scrap Manager clipboard with new data sent from server.
 */
void DoServerCutText (void) {
	unsigned long textLen;
    unsigned long i;

	if (! DoWaitingReadTCP (3)) {	/* Read & ignore padding */
		DoClose(vncWindow);
        return;
        }
	if (! DoWaitingReadTCP (4)) {
		DoClose(vncWindow);
        return;
        }
    HLock(readBufferHndl);
    textLen = SwapBytes4((unsigned long) **readBufferHndl);
    HUnlock(readBufferHndl);

    if (! DoWaitingReadTCP(textLen)) {
	    DoClose(vncWindow);
        return;
        };
	if (allowClipboardTransfers) {
        ZeroScrap();
        HLock(readBufferHndl);
                    
        /* Convert lf->cr; Use pointer arithmetic so we can go over 64k */
        for (i = 0; i < textLen; i++)
	        if (*((*(char **)readBufferHndl)+i) == '\n')
                *((*(char **)readBufferHndl)+i) = '\r';

        			/* Below function call requires <scrap.h> to be fixed */
        PutScrap(textLen, textScrap, (Pointer) *readBufferHndl);
        /* Potential errors (e.g. out of memory) ignored */
        HUnlock(readBufferHndl);
        }
    }

/* Send a DoPointerEvent reflecting the status of the mouse to the server */
void DoPointerEvent (void) {
	static struct {
	    unsigned char messageType;
        unsigned char buttonMask;
        unsigned int xPos;
        unsigned int yPos;
        } pointerEventStruct = { 5 /* message type */ };

    Point mouseCoords;
	unsigned long contentOrigin;
    Point * contentOriginPtr = (void *) &contentOrigin;
    RegionHndl contentRgnHndl;
    unsigned int oldButtonMask;
    GrafPortPtr winPtr;
    unsigned long key1 = 0x0000;	  /* Keys to release & re-press, if any */
    unsigned long key2 = 0x0000;

    if (viewOnlyMode)
	    return;

    mouseCoords = myEvent.where;

    SetPort(vncWindow);

    /* Check if mouse is in content region of VNC window; don't send mouse
     * updates if it isn't.
     */
    if (FindWindow(&winPtr, myEvent.where.h, myEvent.where.v) != wInContent ||
	    		winPtr != vncWindow)
		return;

    GlobalToLocal(&mouseCoords);

	contentOrigin = GetContentOrigin(vncWindow);
    mouseCoords.h += contentOriginPtr->h;
    mouseCoords.v += contentOriginPtr->v;

    mouseCoords.h = SwapBytes2(mouseCoords.h);
    mouseCoords.v = SwapBytes2(mouseCoords.v);

    /* Set up correct state of mouse buttons */
    oldButtonMask = pointerEventStruct.buttonMask;
    pointerEventStruct.buttonMask = 0x00;

    if ((myEvent.modifiers & btn0State) == 0x00) {	/* Mouse button pressed */
		if (emulate3ButtonMouse) {
   	    	if (myEvent.modifiers & optionKey) {
	        	pointerEventStruct.buttonMask = 0x02;
	            key1 = 0xFFE9;
    	        }
        	if (myEvent.modifiers & appleKey) {
	        	pointerEventStruct.buttonMask |= 0x04;
	            key2 = 0xFFE7;
    	        }
        	}

	    /* If no modifiers, just send a normal left click. */
    	if (pointerEventStruct.buttonMask == 0x00)
    		pointerEventStruct.buttonMask = 0x01;
    }
    if ((myEvent.modifiers & btn1State) == 0x00)  	/* If 2nd (right)    */
	    pointerEventStruct.buttonMask |= 0x04;    	/* button is pressed */

    /* Don't waste bandwidth by sending update if mouse hasn't changed.
     * This may occasionally result in an initial mouse update not being
     * sent.  If this occurs, the user can simply move the mouse slightly
     * in order to send it.
     */
    if ( 		(pointerEventStruct.xPos == mouseCoords.h) &&
		 		(pointerEventStruct.yPos == mouseCoords.v) &&
                (pointerEventStruct.buttonMask == oldButtonMask) )
	    return;

	pointerEventStruct.xPos = mouseCoords.h;
    pointerEventStruct.yPos = mouseCoords.v;

    if (key1)
	    SendKeyEvent(FALSE, key1);
    if (key2)
	    SendKeyEvent(FALSE, key2);

    TCPIPWriteTCP(hostIpid, (Pointer) &pointerEventStruct.messageType,
    				sizeof(pointerEventStruct), TRUE, FALSE);
    /* Can't do useful error checking here */

    if (key1)
	    SendKeyEvent(TRUE, key1);
    if (key2)
	    SendKeyEvent(TRUE, key2);

    //printf("Sent mouse update: x = %u, y = %u\n", mouseCoords.h, mouseCoords.v);
    //printf("                   xPos = %x, yPos = %x, buttons = %x\n", pointerEventStruct.xPos, pointerEventStruct.yPos, (int) pointerEventStruct.buttonMask);

    /* Note that we don't have to request a display update here.  That has
     * been or will be done elsewhere when we're ready for it.
     */
}

/* Here when we're done processing one rectangle and ready to start the next.
 * If last FramebufferUpdate had multiple rectangles, we set up for next one.
 * If no more rectangles are available, we send a FramebufferUpdateRequest.
 */
void NextRect (void) {
	unsigned int *dataPtr;

	numRects--;
    if (numRects) {		            /* Process next rectangle */
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
    else {							/* No more rectangles from last update */
		unsigned long contentOrigin;
    	Point * contentOriginPtr = (void *) &contentOrigin;

		contentOrigin = GetContentOrigin(vncWindow);
		SendFBUpdateRequest(TRUE, contentOriginPtr->h, contentOriginPtr->v,
    						winWidth, winHeight);
	    }
    }                         

/* Ends drawing of a raw rectangle when it is complete or aborted
 * because the rectangle is not visible.
 */
void StopRawDrawing (void) {
	HUnlock(readBufferHndl);
    free(srcLocInfo.ptrToPixImage);		/* Allocated as destPtr */

    displayInProgress = FALSE;

    NextRect();										/* Prepare for next rect */
}

#pragma optimize 95		/* To work around an ORCA/C optimizer bug */

/* Draw one or more lines from a raw rectangle
 */
void RawDraw (void) {      
    unsigned int i;			/* Loop indices */
	unsigned char *dataPtr;
    unsigned char *lineDataPtr, *initialLineDataPtr;
    unsigned char *finalDestPtr;
    static EventRecord unusedEventRec;

    /* For use with GetContentOrigin() */
	unsigned long contentOrigin;
   	Point * contentOriginPtr = (void *) &contentOrigin;

    SetPort(vncWindow);							/* Drawing in VNC window */
    dataPtr = (unsigned char *) *readBufferHndl;

    /* Check if what we're drawing is visible, and skip any invisible part
     * by skipping some lines or completely aborting drawing the rectangle.
     */
    if (checkBounds) {
		Rect drawingRect;

		contentOrigin = GetContentOrigin(vncWindow);
        drawingRect.h1 = rectX - contentOriginPtr->h;
        drawingRect.h2 = rectX - contentOriginPtr->h + rectWidth;
        drawingRect.v1 = rectY - contentOriginPtr->v + drawingLine;
        drawingRect.v2 = rectY - contentOriginPtr->v + rectHeight;

        if (!RectInRgn(&drawingRect, GetVisHandle())) {
	    	StopRawDrawing();
            return;
            }
        else if (rectY + drawingLine < contentOriginPtr->v) {
            destPtr += (unsigned long)lineBytes *
            	 (contentOriginPtr->v - rectY - drawingLine);
	        drawingLine = contentOriginPtr->v - rectY;
	
        	if (drawingLine >= rectHeight) {	/* Sanity check */
	        	StopRawDrawing();
            	return;
            	}
            }
        else if (rectY + rectHeight - 1 > contentOriginPtr->v + winHeight)
	        rectHeight = contentOriginPtr->v + winHeight - rectY + 1;

        checkBounds = FALSE;
        }

    lineDataPtr = dataPtr + (unsigned long) drawingLine * rectWidth;
	        
    do {	/* We short-circuit back to here if there are no events pending */

    	finalDestPtr = destPtr + lineBytes - 1;
        if (hRez == 640) {
	        initialLineDataPtr = lineDataPtr;
            while (destPtr + 7 < finalDestPtr) {	/* Unrolled loop */
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            }
	        while (destPtr < finalDestPtr) {
                *(destPtr++) = bigcoltab640a[*(unsigned int*)(void*)lineDataPtr]
						+ bigcoltab640b[*(unsigned int*)(void*)(lineDataPtr+2)];
                lineDataPtr += 4;
	            }
            /* Final byte to produce */
            *destPtr      = pixTransTbl[*(lineDataPtr++)] & 0xC0;
	   		for (i = lineDataPtr - initialLineDataPtr; i < rectWidth; i++)
		    	switch (i & 0x03) {
           			case 0x01:			/* pixels 1, 5, 9, ... */
        	   	      	*destPtr     += pixTransTbl[*(lineDataPtr++)] & 0x30;
                    	break;
					case 0x02:      	/* pixels 2, 6, 10, ... */
                    	*destPtr     += pixTransTbl[*(lineDataPtr++)] & 0x0C;
        	   	        break;
           			case 0x03:			/* pixels 3, 7, 11, ... */
		    			*destPtr     += pixTransTbl[*(lineDataPtr++)] & 0x03;
	                }
            destPtr++;
            }
	    else {			/* 320 mode */
	        while (destPtr + 7 < finalDestPtr) {	/* Unrolled loop */
            	*(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
	            }
    		while (destPtr < finalDestPtr) {
                *(destPtr++) = bigcoltab320[*(unsigned int*)(void*)lineDataPtr];
                lineDataPtr += 2;
                }
            /* Final byte to produce */
            *destPtr      = pixTransTbl[*(lineDataPtr++)] & 0xF0;
	        if (extraByteAdvance)
	            destPtr++;	/* Not ending on byte boundary - update index */
            else
               	*(destPtr++) += pixTransTbl[*(lineDataPtr++)] & 0x0F;
            }
	        	
    	drawingLine++;

        if (pixels > 613 && !(drawingLine & 0x03)) {  /* Draw every 4th line */
        	srcRect.v2 = drawingLine;
	    	contentOrigin = GetContentOrigin(vncWindow);
			PPToPort(&srcLocInfo, &srcRect, rectX - contentOriginPtr->h,
	    		rectY + srcRect.v1 - contentOriginPtr->v, modeCopy);
        	srcRect.v1 = drawingLine;
        	}

        /* Check whether we're done with this rectangle */
        if (drawingLine >= rectHeight) {
	        /* Draw final rect, if necessary */
	        if (drawingLine > srcRect.v1) {
            	srcRect.v2 = drawingLine;
	    		contentOrigin = GetContentOrigin(vncWindow);
				PPToPort(&srcLocInfo, &srcRect, rectX - contentOriginPtr->h,
	    			rectY + srcRect.v1 - contentOriginPtr->v, modeCopy);
	            }
	        StopRawDrawing();
            return;
            }

        /* Check if there are actually any events that need to be processed.
         * If not, save time by not going through the whole event loop, but
         * instead processing the minimum necessary periodic tasks and then
         * going straight to the next line of data.
         */
        if (EventAvail(0xFFFF, &unusedEventRec))
            return;

	    SystemTask();	/* Let periodic Desk Accesories do their things */
        TCPIPPoll();	/* Let Marinetti keep processing data */

    	} while (1);
}

#pragma optimize -1

/* Draw one line of Raw data - used if the complete rect isn't yet available */
void RawDrawLine (void) {
	unsigned int i;
	unsigned char *dataPtr;
	unsigned long contentOrigin;
   	Point * contentOriginPtr = (void *) &contentOrigin;

    if (hRez == 640) {
    	if (rectWidth & 0x03)			/* Width not an exact multiple of 4 */
	    	lineBytes = rectWidth/4 + 1;
    	else							/* Width is a multiple of 4 */
	    	lineBytes = rectWidth/4;
        }
    else {	/* 320 mode */
    	if (rectWidth & 0x01)			/* Width not an exact multiple of 2 */
	    	lineBytes = rectWidth/2 + 1;
    	else							/* Width is a multiple of 2 */
	    	lineBytes = rectWidth/2;
        }

    destPtr = calloc(lineBytes, 1);
    if (!destPtr) {						/* Couldn't allocate memory */
		DoClose(vncWindow);
        return;
        }

    srcLocInfo.ptrToPixImage = destPtr;
    srcLocInfo.width = lineBytes;
    srcLocInfo.boundsRect.v2 = 1;
    /* Since the lines are rounded up to integral numbers of bytes, this
     * padding must be accounted for here.
     */
    if (hRez == 640) {
    	switch (rectWidth & 0x03) {
	    	case 0x00:	srcLocInfo.boundsRect.h2 = rectWidth;		break;
	        case 0x01:	srcLocInfo.boundsRect.h2 = rectWidth+3;		break;
    	    case 0x02:	srcLocInfo.boundsRect.h2 = rectWidth+2;		break;
       		case 0x03:	srcLocInfo.boundsRect.h2 = rectWidth+1;
        	}
        }
    else {
    	switch (rectWidth & 0x01) {
	    	case 0x00:	srcLocInfo.boundsRect.h2 = rectWidth;		break;
	        case 0x01:	srcLocInfo.boundsRect.h2 = rectWidth+1;
        	}
        }	

    /* Don't include padding in the area we will actually copy over */
    srcRect.h2 = rectWidth;
   	srcRect.v1 = 0;
	srcRect.v2 = 1;

    HLock(readBufferHndl);
    dataPtr = (unsigned char *) *readBufferHndl;
    SetPort(vncWindow);							/* Drawing in VNC window */

	if (hRez == 640)
		for (i = 0; i < rectWidth; /* i is incremented in loop */) {
		   	switch (i & 0x03) {
		       	case 0x00:			/* pixels 0, 4, 8, ... */
			    	*destPtr      = pixTransTbl[dataPtr[i++]] & 0xC0;
           	        break;
        		case 0x01:			/* pixels 1, 5, 9, ... */
		  			*destPtr     += pixTransTbl[dataPtr[i++]] & 0x30;
                   	break;
				case 0x02:      	/* pixels 2, 6, 10, ... */
			    	*destPtr     += pixTransTbl[dataPtr[i++]] & 0x0C;
           	        break;
        		case 0x03:			/* pixels 3, 7, 11, ... */
		   			*(destPtr++) += pixTransTbl[dataPtr[i++]] & 0x03;
	            }
    	   	}
	else			/* 320 mode */
    	for (i = 0; i < rectWidth; /* i is incremented in loop */) {
       	    if ((i & 0x01) == 0)		    /* pixels 0, 2, 4, ... */
				*destPtr      = pixTransTbl[dataPtr[i++]] & 0xF0;
	        else {					/* pixels 1, 3, 5, ... */
                *(destPtr++) += pixTransTbl[dataPtr[i++]] & 0x0F;
                }
	       	}

    HUnlock(readBufferHndl);
	contentOrigin = GetContentOrigin(vncWindow);
	PPToPort(&srcLocInfo, &srcRect, rectX - contentOriginPtr->h,
	   		rectY - contentOriginPtr->v, modeCopy);
    free(srcLocInfo.ptrToPixImage);		/* Allocated as destPtr */

    TCPIPPoll();

    rectHeight--;	/* One less line left to draw */
    rectY++;		/* Rest of rect starts one line below this */
    }

/* Process rectangle data in raw encoding and write it to screen.
 */
void DoRawRect (void) {
    unsigned long bufferLength;

    pixels = (unsigned long) rectWidth * rectHeight;

    /* Try to read data */
	if (! DoReadTCP (pixels)) {
	    /* Only support line-by-line drawing if the connection is quite slow;
         * otherwise it's actually detrimental to overall speed.  The Hextile
         * setting is used as a hint at the connection speed.
         */
	    if (useHextile && rectHeight > 1 && DoReadTCP ((unsigned long) rectWidth))
	        RawDrawLine();			/* Some data ready - draw first line */
        return;							/* Not ready yet; wait */
        }

    /* Here if data is ready to be processed */

    if (hRez == 640) {
    	if (rectWidth & 0x03) {			/* Width not an exact multiple of 4 */
	    	lineBytes = rectWidth/4 + 1;
            extraByteAdvance = TRUE;
        	}
    	else {							/* Width is a multiple of 4 */
	    	lineBytes = rectWidth/4;
            extraByteAdvance = FALSE;
        	}
        }
    else {	/* 320 mode */
    	if (rectWidth & 0x01) {			/* Width not an exact multiple of 2 */
	    	lineBytes = rectWidth/2 + 1;
            extraByteAdvance  = TRUE;
        	}
    	else {							/* Width is a multiple of 2 */
	    	lineBytes = rectWidth/2;
            extraByteAdvance = FALSE;
        	}
        }

    bufferLength = lineBytes * rectHeight;
    destPtr = calloc(bufferLength, 1);
    if (!destPtr) {						/* Couldn't allocate memory */
		DoClose(vncWindow);
        return;
        }

    srcLocInfo.ptrToPixImage = destPtr;
    srcLocInfo.width = lineBytes;
    srcLocInfo.boundsRect.v2 = rectHeight;
    /* Since the lines are rounded up to integral numbers of bytes, this
     * padding must be accounted for here.
     */
    if (hRez == 640) {
    	switch (rectWidth & 0x03) {
	    	case 0x00:	srcLocInfo.boundsRect.h2 = rectWidth;		break;
	        case 0x01:	srcLocInfo.boundsRect.h2 = rectWidth+3;		break;
    	    case 0x02:	srcLocInfo.boundsRect.h2 = rectWidth+2;		break;
       		case 0x03:	srcLocInfo.boundsRect.h2 = rectWidth+1;
        	}
        }
    else {
    	switch (rectWidth & 0x01) {
	    	case 0x00:	srcLocInfo.boundsRect.h2 = rectWidth;		break;
	        case 0x01:	srcLocInfo.boundsRect.h2 = rectWidth+1;
        	}
        }	

    /* Don't include padding in the area we will actually copy over */
    srcRect.h2 = rectWidth;
    srcRect.v1 = 0;

	displayInProgress = TRUE;
    drawingLine = 0;			/* Drawing first line of rect */
    checkBounds = TRUE;			/* Flag to check bounds when drawing 1st line */
    HLock(readBufferHndl);		/* Lock handle just once for efficiency */
    }

void DoCopyRect (void) {
    /* For use with GetContentOrigin() */
	unsigned long contentOrigin;
   	Point * contentOriginPtr = (void *) &contentOrigin;
	
    Rect srcRect;
    unsigned int *dataPtr;				/* Pointer to TCP data that was read */

    //printf("Processing CopyRect rectangle\n");

	if (! DoReadTCP ((unsigned long) 4))
		return;										/* Not ready yet; wait */

    contentOrigin = GetContentOrigin(vncWindow);

    HLock(readBufferHndl);
	dataPtr = (unsigned int *) ((char *) (*readBufferHndl));
    srcRect.h1 = SwapBytes2(dataPtr[0]) - contentOriginPtr->h;
    srcRect.v1 = SwapBytes2(dataPtr[1]) - contentOriginPtr->v;
    HUnlock(readBufferHndl);

    srcRect.h2 = srcRect.h1 + rectWidth;
    srcRect.v2 = srcRect.v1 + rectHeight;

    /* Check that the source rect is actually visible; if not, ask the server
       to send the update using some other encoding.
     */
    if (!RectInRgn(&srcRect, GetVisHandle())) {
	    SendFBUpdateRequest(FALSE, rectX, rectY, rectWidth, rectHeight);
        displayInProgress = FALSE;
        return;
        }

    /* We can use the window pointer as a LocInfo pointer because it starts
     * with a grafPort structure, which in turn starts with a LocInfo structure.
     */
	PPToPort((struct LocInfo *) vncWindow, &srcRect,
    		rectX - contentOriginPtr->h, rectY - contentOriginPtr->v, modeCopy);

    displayInProgress = FALSE;

    NextRect();										/* Prepare for next rect */
    }

void HexNextTile (void) {
    hexXTileNum++;
    if (hexXTileNum == hexXTiles) {
        hexYTileNum++;
        if (hexYTileNum == hexYTiles) {	    /* Done with this Hextile rect */
	        displayInProgress = FALSE;
            NextRect();
            return;
            }                      
	    hexXTileNum = 0;
        }

    hexTileWidth = (hexXTileNum == hexXTiles - 1) ?
    	rectWidth - 16 * (hexXTiles - 1) : 16;
    hexTileHeight = (hexYTileNum == hexYTiles - 1) ?
    	rectHeight - 16 * (hexYTiles - 1) : 16;

	}

void HexRawDraw (Point *contentOriginPtr, int rectWidth, int rectHeight) {      
    unsigned int i, j;			/* Loop indices */
    unsigned int n = 0;
	unsigned char *dataPtr;
    unsigned char pixels[128];

    static Rect srcRect = {0,0,0,0};

    dataPtr = (unsigned char *) *readBufferHndl;

    if ((hRez==640 && (rectWidth & 0x03)) || (hRez==320 && (rectWidth & 0x01)))
            extraByteAdvance = TRUE;
    else
            extraByteAdvance = FALSE;

    for (j = 0; j < rectHeight; j++) {
	   	for (i = 0; i < rectWidth; i++) {
    	    if (hRez == 640) {
	    		switch (i & 0x03) {
	        		case 0x00:			/* pixels 0, 4, 8, ... */
			    		pixels[n] = pixTransTbl[ *(dataPtr +
                    			(unsigned long) j*rectWidth + i)
                        	    ] & 0xC0;
	           	        break;
    	       		case 0x01:			/* pixels 1, 5, 9, ... */
			    		pixels[n] += pixTransTbl[ *(dataPtr +
            	           		(unsigned long) j*rectWidth + i)
		        	            ] & 0x30;
                    	break;
					case 0x02:      	/* pixels 2, 6, 10, ... */
				    	pixels[n] += pixTransTbl[ *(dataPtr +
        	               		(unsigned long) j*rectWidth + i)
								] & 0x0C;
           	    	    break;
	           		case 0x03:			/* pixels 3, 7, 11, ... */
			    		pixels[n] += pixTransTbl[ *(dataPtr +
        	               		(unsigned long) j*rectWidth + i)
								] & 0x03;
                	    n++;
	                } /* switch */
    	       	} /* if */
        	else {			/* 320 mode */
            	switch(i & 0x01) {
                	case 0x00:		/* pixels 0, 2, 4, ... */
						pixels[n] = pixTransTbl[ *(dataPtr +
                       			(unsigned long) j*rectWidth + i)
                            	] & 0xF0;
	                       break;
    	            case 0x01:		/* pixels 1, 3, 5, ... */
        	           	pixels[n] += pixTransTbl[ *(dataPtr +
            	           		(unsigned long) j*rectWidth + i)
                	            ] & 0x0F;
                    	n++;
	                } /* switch */
		        } /* else */
       		} /* i loop */

	    /* When not ending a line on a byte boundary, the index isn't updated,
    	 * so we do it here.
	     */
    	if (extraByteAdvance)
	    	n++;
    	} /* j loop */

    srcLocInfo.ptrToPixImage = (void *) pixels;                     
    srcLocInfo.boundsRect.v2 = rectHeight;
    /* Since the lines are rounded up to integral numbers of bytes, this
     * padding must be accounted for here.
     */
    if (hRez == 640) {
    	switch (rectWidth & 0x03) {
	    	case 0x00:	srcLocInfo.boundsRect.h2 = rectWidth;
			            srcLocInfo.width = rectWidth/4;				break;
	        case 0x01:	srcLocInfo.boundsRect.h2 = rectWidth+3;	
            			srcLocInfo.width = rectWidth/4 + 1;			break;
    	    case 0x02:	srcLocInfo.boundsRect.h2 = rectWidth+2;		
            			srcLocInfo.width = rectWidth/4 + 1;			break;
       		case 0x03:	srcLocInfo.boundsRect.h2 = rectWidth+1;
			            srcLocInfo.width = rectWidth/4 + 1;
        	}
        }
    else { /* hRez == 320 */
    	switch (rectWidth & 0x01) {
	    	case 0x00:	srcLocInfo.boundsRect.h2 = rectWidth;
            			srcLocInfo.width = rectWidth/2;				break;
	        case 0x01:	srcLocInfo.boundsRect.h2 = rectWidth+1;
			            srcLocInfo.width = rectWidth/2 + 1;
        	}
        }	

    srcRect.v2 = hexTileHeight;                                     
    srcRect.h2 = hexTileWidth;                                      

    PPToPort(&srcLocInfo, &srcRect,								
     	rectX + hexXTileNum * 16 - contentOriginPtr->h,             
        rectY + hexYTileNum * 16 - contentOriginPtr->v, modeCopy);
}

/* The macros below are used in HexDispatch() */
#define HexDispatch_NextTile()	do {								\
    HexNextTile();                                                  \
    HUnlock(readBufferHndl);										\
    /* Set up for next time */                                      \
    status = hexWaitingForSubencoding;                              \
    bytesNeeded = 1;                                                \
    return;								                            \
    } while (0)

#define HexDispatch_DrawRect(color, X, Y, width, height)	do {		\
	SetSolidPenPat((color));   \
    drawingRect.h1 = rectX + hexXTileNum * 16 + (X) - contentOriginPtr->h;   \
    drawingRect.v1 = rectY + hexYTileNum * 16 + (Y) - contentOriginPtr->v;    \
    drawingRect.h2 = rectX + hexXTileNum * 16 + (X) + (width) - contentOriginPtr->h; \
    drawingRect.v2 = rectY + hexYTileNum * 16 + (Y) + (height) - contentOriginPtr->v; \
    PaintRect(&drawingRect);                                          \
	} while (0)

#define HexDispatch_DrawBackground() \
	HexDispatch_DrawRect(hexBackground, 0, 0, hexTileWidth, hexTileHeight)

void HexDispatch (void) {
	static unsigned char status = hexWaitingForSubencoding;
    static unsigned long bytesNeeded = 1;
    static unsigned char subencoding;
    static unsigned int numSubrects;
    int i;
    /* For use with GetContentOrigin() */
	unsigned long contentOrigin;
   	Point * contentOriginPtr = (void *) &contentOrigin;
    int tileBytes;
    unsigned int srX, srY, srWidth, srHeight;
    Rect drawingRect;
    static unsigned char pixels[128];
    unsigned char *dataPtr;

    contentOrigin = GetContentOrigin(vncWindow);
    SetPort(vncWindow);

    /* If we don't have the next bit of needed data yet, return. */
    while (DoReadTCP(bytesNeeded)) {
        HLock(readBufferHndl);
        dataPtr = *(unsigned char **) readBufferHndl;
   		/* If we're here, readBufferHndl contains bytesNeeded bytes of data. */
    	switch (status) {
	    	case hexWaitingForSubencoding:
                subencoding = *dataPtr;
	        	if (subencoding & Raw) {
	            	bytesNeeded = hexTileWidth * hexTileHeight;
                    status = hexWaitingForRawData;
    	            }
	    	    else {
	                bytesNeeded = 0;
                	if (subencoding & BackgroundSpecified)		
        				bytesNeeded++;	                        
				    if (subencoding & ForegroundSpecified)   
				       	bytesNeeded++;                            
		    		if (subencoding & AnySubrects)           
					    bytesNeeded++;                            
                	else if (bytesNeeded == 0) {
	                    /* No more data - just draw background */
                        HexDispatch_DrawBackground();
                		HexDispatch_NextTile();
                        }
                    status = hexWaitingForMoreInfo;
                    }
    	        break;

        	case hexWaitingForRawData:
	        	HexRawDraw(contentOriginPtr, hexTileWidth, hexTileHeight);
                HexDispatch_NextTile();
    	        break;

            case hexWaitingForMoreInfo:
				if (subencoding & BackgroundSpecified) {
			    	hexBackground = pixTransTbl[*(dataPtr++)];
                	}
				if (subencoding & ForegroundSpecified) {
                    hexForeground = pixTransTbl[*(dataPtr++)];
            	    }
				if (subencoding & AnySubrects) {
			        numSubrects = *dataPtr;
        		    if (numSubrects) {
	        		    status = hexWaitingForSubrect;
                		bytesNeeded = numSubrects * ((subencoding & SubrectsColoured) ? 3 : 2);
	                	break;
                        }
	    	        else
		    	        HexDispatch_NextTile();
            	    }
                else {	/* no subrects */
	                HexDispatch_DrawBackground();
                    HexDispatch_NextTile();
                	}

			case hexWaitingForSubrect: {
            	HexDispatch_DrawBackground();
                while (numSubrects-- > 0) {
	            	if (subencoding & SubrectsColoured) {
	    	        	hexForeground = pixTransTbl[*(dataPtr++)];
                    	}
	                srX = *dataPtr >> 4;
    	            srY = *(dataPtr++) & 0x0F;
        	        srWidth = (*dataPtr >> 4) + 1;
            	    srHeight = (*(dataPtr++) & 0x0F) + 1;
                	HexDispatch_DrawRect(hexForeground, srX, srY, srWidth, srHeight);
    	            }
        	    HexDispatch_NextTile();
	            }
        	}
        	HUnlock(readBufferHndl);
        }
	}

/* Called when we initially get a Hextile rect; set up to process it */
void DoHextileRect (void) {
	hexXTiles = (rectWidth + 15) / 16;
    hexYTiles = (rectHeight + 15) / 16;

    hexXTileNum = 0;
    hexYTileNum = 0;

    displayInProgress = TRUE;
	
    hexTileWidth = (hexYTileNum == hexXTiles - 1) ?
    	rectWidth - 16 * (hexXTiles - 1) : 16;
    hexTileHeight = (hexYTileNum == hexYTiles - 1) ?
    	rectHeight - 16 * (hexYTiles - 1) : 16;

    /* Set up for Hextile drawing */
    srcRect.v1 = 0;                                                 
    srcRect.h1 = 0;                                                
    }

/* This prototype should be in <window.h> but is bogusly commented out there */
extern pascal void SetContentOrigin2(Word, Word, Word, GrafPortPtr) inline(0x570E,dispatcher);

void DoDesktopSize (void) {
	#define screenTooBigError	2010
    unsigned long contentOrigin;
   	Point * contentOriginPtr = (void *) &contentOrigin;
    unsigned int newX, newY;
    Boolean changeOrigin = FALSE;
    unsigned int oldWinHeight, oldWinWidth;

    fbWidth = rectWidth;
    fbHeight = rectHeight;

    if ((fbWidth > 16384) || (fbHeight > 16384)) {
        AlertWindow(awResource, NULL, screenTooBigError);
		DoClose(vncWindow);
        }

    oldWinHeight = winHeight;
    oldWinWidth = winWidth;
    winHeight = 174;
    winWidth = (hRez == 640) ? 613 : 302;
    if (fbWidth < winWidth)
		winWidth = fbWidth;
    if (fbHeight < winHeight)
	    winHeight = fbHeight;
    if (oldWinHeight != winHeight || oldWinWidth != winWidth)
    	SizeWindow(winWidth, winHeight, vncWindow);
    
    /* Scroll if area displayed is going away */
    contentOrigin = GetContentOrigin(vncWindow);
    newX = contentOriginPtr->h;
    newY = contentOriginPtr->v;

    if (contentOriginPtr->h + winWidth > fbWidth) {
	    newX = fbWidth - winWidth;
        changeOrigin = TRUE;
        }
    if (contentOriginPtr->v + winHeight > fbHeight) {
	    newY = fbHeight - winHeight;
        changeOrigin = TRUE;
        }
    SetContentOrigin2(1, newX, newY, vncWindow);

    SetDataSize(fbWidth, fbHeight, vncWindow);
    DrawControls(vncWindow);

    displayInProgress = FALSE;

    NextRect();				/* Prepare for next rect */
	}

void ConnectedEventLoop (void) {
	unsigned char messageType;
#define FBUpdate			0
#define SetColourMapEntries 1
#define Bell				2
#define ServerCutText		3

	if (FrontWindow() != vncWindow && menuOffset == noKB)
	    InitMenus(0);
    else if (FrontWindow() == vncWindow && menuOffset != noKB)
	    InitMenus(noKB);

	if (displayInProgress) {
	    switch (rectEncoding) {
	        case encodingRaw:	RawDraw();
					            return;
            case encodingHextile: HexDispatch();
					            return;
            default:			DoClose(vncWindow);
					            return;
			}
        }
	else if (numRects) {
	    switch (rectEncoding) {
	        case encodingHextile:
					            DoHextileRect();
                                return;
	    	case encodingRaw:	DoRawRect();
					            return;
            case encodingCopyRect:
					            DoCopyRect();
	                            return;
            case encodingDesktopSize:
					            DoDesktopSize();
					            return;
            default:			DisplayConnectStatus (
            						"\pInvalid rectangle from server.", FALSE);
                                DoClose(vncWindow);
                                //printf("Closing due to bad rectangle encoding %lu\n", rectEncoding);
                                //printf("rectX = %u, rectY = %u, rectWidth = %u, rectHeight = %u\n", rectX, rectY, rectWidth, rectHeight);
                                return;
            }
	    }
	else if (DoReadTCP(1)) {			/* Read message type byte */
	    HLock(readBufferHndl);
    	messageType = ((unsigned char) **readBufferHndl);
        HUnlock(readBufferHndl);
    	switch (messageType) {
	    	case FBUpdate:              DoFBUpdate();
								        break;
	    	case SetColourMapEntries:	DoSetColourMapEntries();
        								break;
        	case Bell:					SysBeep();
								        break;
        	case ServerCutText:			DoServerCutText();
        								break;
        	default:					DisplayConnectStatus (
        									"\pInvalid message from server.",
                                        	FALSE);
                                        DoClose(vncWindow);
            	                        //printf("Closing due to bad message from server\n");
                                        return;
        	}
        }
    }

void DoSendClipboard (void) {
	static struct clientCutText {
	    unsigned char messageType;
        unsigned char padding1;
        unsigned int  padding2;
        unsigned long length;
       	} clientCutTextStruct = { 6 /* Message type 6 */ };

    Handle scrapHandle;
    unsigned long i;

	/* Only proceed if we're connected to the server and not view-only */
	if (vncConnected && !viewOnlyMode) {
		clientCutTextStruct.length = GetScrapSize(textScrap);

        if (clientCutTextStruct.length == 0)
	        return;
	
        clientCutTextStruct.length = SwapBytes4(clientCutTextStruct.length);
       	
        scrapHandle = NewHandle(1, userid(), 0x0000, NULL);
        GetScrap(scrapHandle, textScrap);
        if (toolerror())
	        goto end;						/* abort if error */
    	if (TCPIPWriteTCP(hostIpid, &clientCutTextStruct.messageType,
    				sizeof(clientCutTextStruct), FALSE, FALSE))
		    goto end;						/* abort if error */
        if (toolerror())
	        goto end;
	
	    clientCutTextStruct.length = SwapBytes4(clientCutTextStruct.length);

        HLock(scrapHandle);
        /* Convert cr->lf; Use pointer arithmetic so we can go over 64k */
        for (i = 0; i < clientCutTextStruct.length; i++)
	        if (*((*(char **)scrapHandle)+i) == '\r')
                *((*(char **)scrapHandle)+i) = '\n';

		TCPIPWriteTCP(hostIpid, (Pointer) *scrapHandle,
        			clientCutTextStruct.length, TRUE, FALSE);
        /* Can't handle errors usefully here */
        HUnlock(scrapHandle);

        end:
        DisposeHandle(scrapHandle);
        }
    }

/* Process a key down event and send it on to the server. */
void ProcessKeyEvent (void)
{
	unsigned long key = myEvent.message & 0x0000007F;

    if (viewOnlyMode)
	    return;

    /* Deal with extended keys that are mapped as keypad keys */
    if (myEvent.modifiers & keyPad) {
	    switch (key) {
	        case 0x7A:	key = 0xFFBE;	break;	/* F1 */
            case 0x78:  key = 0xFFBF;   break;  /* F2 */
            case 0x63:  key = 0xFFC0;   break;  /* F3 */
            case 0x76:  key = 0xFFC1;   break;  /* F4 */
            case 0x60:  key = 0xFFC2;   break;  /* F5 */
            case 0x61:  key = 0xFFC3;   break;  /* F6 */
            case 0x62:  key = 0xFFC4;   break;  /* F7 */
            case 0x64:  key = 0xFFC5;   break;  /* F8 */
            case 0x65:  key = 0xFFC6;   break;  /* F9 */
            case 0x6D:  key = 0xFFC7;   break;  /* F10 */
            case 0x67:  key = 0xFFC8;   break;  /* F11 */
            case 0x6F:  key = 0xFFC9;   break;  /* F12 */
            case 0x69:  key = 0xFF15;   break;	/* F13 / PrintScr -> SysRq */
            case 0x6B:  key = 0xFF14;   break;  /* F14 / ScrLock -> ScrLock */
            case 0x71:  key = 0xFF13;   break;  /* F15 / Pause -> Pause */
            case 0x72:  key = 0xFF63;   break;  /* Help / Insert -> Insert */
            case 0x75:  key = 0xFFFF;   break;  /* Forward delete -> Delete */
            case 0x73:  key = 0xFF50;   break;  /* Home */
            case 0x77:  key = 0xFF57;   break;  /* End */
            case 0x74:  key = 0xFF55;   break;  /* Page Up */
            case 0x79:  key = 0xFF56;   break;  /* Page Down */
	        }
        }

    if (key == 0x7f)
	    key = 0xFF08;	/* Delete -> BackSpace */

    if (key < 0x20) {
	    if (myEvent.modifiers & controlKey) {
	    	if (((myEvent.modifiers & shiftKey) ||
            								(myEvent.modifiers & capsLock))
	        				&& !((myEvent.modifiers & shiftKey) &&
                            				(myEvent.modifiers & capsLock)))
            	key += 0x40;   /* Undo effect of control on upper-case char. */
        	else
	        	key += 0x60;   /* Undo effect of control */
	        }
        else switch (key) {
	        case 0x1B:	key = 0xFF1B;	break;	/* Escape */
            case 0x09:	key = 0xFF09;   break;	/* Tab */
            case 0x0D:	key = 0xFF0D;   break;	/* Return / Enter */
            case 0x08:	key = 0xFF51;   break;	/* Left arrow */
            case 0x0B:	key = 0xFF52;   break;	/* Up arrow */
            case 0x15:	key = 0xFF53;   break;	/* Right arrow */
            case 0x0A:	key = 0xFF54;   break;	/* Down arrow */
            case 0x18:	key = 0xFF0B;	break;	/* Clear / NumLock -> Clear */
	        }
        }

    /* Test if we seem to have a valid character and return if we don't.
       This should never return, unless there are bugs in this routine or
       TaskMaster gives us bogus keycodes.  The test would need to be updated
       if we ever start generating valid keycodes outside of these ranges.
     */
    if ((key & 0xFF80) != 0xFF00 && (key & 0xFF80) != 0x0000)
	    return;

    SendKeyEvent(TRUE, key);
    SendKeyEvent(FALSE, key);
}

/* Send modifier keys that have changed since last update */
void SendModifiers (void) {
	static unsigned int oldModifiers = 0x00FF; /* So it runs 1st time */
    unsigned int modifiers;

    modifiers = myEvent.modifiers & 0x1B00;

    /* If unchanged, do nothing. */
    if (modifiers == oldModifiers)
	    return;

    /* Apple key is sent as "meta" */
    if ((modifiers & appleKey) != (oldModifiers & appleKey))
	    SendKeyEvent(modifiers & appleKey, 0xFFE7);

    if ((modifiers & shiftKey) != (oldModifiers & shiftKey))
	    SendKeyEvent(modifiers & shiftKey, 0xFFE1);

    /* Option key is sent as "alt," as per its labelling on some keyboards */
    if ((modifiers & optionKey) != (oldModifiers & optionKey))
	    SendKeyEvent(modifiers & optionKey, 0xFFE9);

    if ((modifiers & controlKey) != (oldModifiers & controlKey))
	    SendKeyEvent(modifiers & controlKey, 0xFFE3);

    oldModifiers = modifiers;
}
