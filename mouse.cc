#pragma noroot

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

unsigned char * cursor = NULL;	/* Cursor from server */

/* Send a DoPointerEvent reflecting the status of the mouse to the server */
/* This routine also maintains the appropriate cursor when using local cursor */
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
	    		winPtr != vncWindow) {
    	if (cursor && GetCursorAdr() == cursor)
	    	InitCursor();
		return;
        }

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

    if (cursor && GetCursorAdr() != cursor)
	    SetCursor(cursor);
}

void DoCursor (void) {
    unsigned char *cursorPixels;
    unsigned char *bitmask;
    unsigned char *dataPtr;
    /* Elements of the cursor structure (which isn't a C struct) */
    unsigned int *cursorHeightPtr, *cursorWidthPtr;
    unsigned char *cursorImage, *cursorMask;
    unsigned int *hotSpotYPtr, *hotSpotXPtr;
    unsigned long bitmaskByte;
    unsigned long bitmaskLineBytes, lineWords;
    unsigned int line, n, j;	/* Loop counters */
    unsigned char *maskLine, *imageLine;
    unsigned char *oldCursor = cursor;	/* So we can free() it later */
    unsigned int outBytes640;
    unsigned long outBytes320;

    bitmaskLineBytes = (rectWidth + 7) / 8;

    if (!DoReadTCP(rectWidth*rectHeight + bitmaskLineBytes*rectHeight))
	    return;	/* Try again later */

    HLock(readBufferHndl);

    cursorPixels = (unsigned char *)(*readBufferHndl);
    bitmask = (unsigned char *)(*readBufferHndl) + rectWidth*rectHeight;

    if (hRez == 640)
    	lineWords = (rectWidth + 7) / 8 + 1;
    else /* hRez == 320 */
        lineWords = (rectWidth + 3) / 4 + 1;

    cursor = malloc(8 + 4 * lineWords * rectHeight);
    /* Sub-optimal error handling */
    if (cursor == NULL)
	    return;
    /* Don't overflow loop indices */
    if ((lineWords > UINT_MAX) || (rectHeight > UINT_MAX))	
	    return;
    cursorHeightPtr = (unsigned int *)(void *)cursor;
    cursorWidthPtr  = cursorHeightPtr + 1;
    cursorImage     = cursor + 4;
    cursorMask      = cursorImage + lineWords * rectHeight * 2;
    hotSpotYPtr     = (unsigned int *)(cursorMask + lineWords * rectHeight * 2);
    hotSpotXPtr     = hotSpotYPtr + 1;

    *cursorHeightPtr = rectHeight;
    *cursorWidthPtr = lineWords;
    *hotSpotYPtr = rectY;
    *hotSpotXPtr = rectX;

    /* Make cursorImage using translation tables */
    /* Make cursorMask from bitmask */

    dataPtr = cursorPixels;

    if (hRez == 320) {
      for (line = 0; line < rectHeight; line++) {	/* for each line ... */
        maskLine = cursorMask + line * lineWords * 2;
        imageLine = cursorImage + line * lineWords * 2;

	   	for (j = 0; j < bitmaskLineBytes; j++) {
          bitmaskByte = *(bitmask + line*bitmaskLineBytes + j);
          outBytes320 =
            	((bitmaskByte & 0x80)      ) + ((bitmaskByte & 0x80) >>  1) +
                ((bitmaskByte & 0x80) >>  2) + ((bitmaskByte & 0x80) >>  3) +
               	((bitmaskByte & 0x40) >>  3) + ((bitmaskByte & 0x40) >>  4) +
                ((bitmaskByte & 0x40) >>  5) + ((bitmaskByte & 0x40) >>  6) +
                ((bitmaskByte & 0x20) << 10) + ((bitmaskByte & 0x20) <<  9) +
                ((bitmaskByte & 0x20) <<  8) + ((bitmaskByte & 0x20) <<  7) +
                ((bitmaskByte & 0x10) <<  7) + ((bitmaskByte & 0x10) <<  6) +
                ((bitmaskByte & 0x10) <<  5) + ((bitmaskByte & 0x10) <<  4) +
                ((bitmaskByte & 0x08) << 20) + ((bitmaskByte & 0x08) << 19) +
                ((bitmaskByte & 0x08) << 18) + ((bitmaskByte & 0x08) << 17) +
                ((bitmaskByte & 0x04) << 17) + ((bitmaskByte & 0x04) << 16) +
                ((bitmaskByte & 0x04) << 15) + ((bitmaskByte & 0x04) << 14) +
                ((bitmaskByte & 0x02) << 30) + ((bitmaskByte & 0x02) << 29) +
                ((bitmaskByte & 0x02) << 28) + ((bitmaskByte & 0x02) << 27) +
                ((bitmaskByte & 0x01) << 27) + ((bitmaskByte & 0x01) << 26) +
                ((bitmaskByte & 0x01) << 25) + ((bitmaskByte & 0x01) << 24);
          *((unsigned long *)maskLine + j) = outBytes320;
          }
        *((unsigned int *)maskLine + lineWords - 1) = 0;

        for (n = 0; n < rectWidth/2; n++) {
          *(imageLine + n)  = coltab320[*(dataPtr++)] & 0xF0;
          *(imageLine + n) += coltab320[*(dataPtr++)] & 0x0F;
          *(imageLine + n) ^= 0xFF;	    /* Reverse color */
          *(imageLine + n) &= *(maskLine + n);
          }
        if (rectWidth % 2) {
	      *(imageLine + n)  = coltab320[*(dataPtr++)] & 0xF0;
          *(imageLine + n) ^= 0xFF;	    /* Reverse color */
          *(imageLine + n) &= *(maskLine + n);
          n++;
          }
        *(imageLine + n) = 0;
        *((unsigned int *)imageLine + lineWords - 1) = 0;
        }
      }
    else { /* hRez == 640 */
	  for (line = 0; line < rectHeight; line++) {	/* for each line ... */
        maskLine = cursorMask + line * lineWords * 2;
        imageLine = cursorImage + line * lineWords * 2;

        for (j = 0; j < bitmaskLineBytes; j++) {      
          bitmaskByte = *(bitmask + line*bitmaskLineBytes + j);
          outBytes640 =
		        ((bitmaskByte & 0x80)      ) + ((bitmaskByte & 0xC0) >>  1) + 
		        ((bitmaskByte & 0x60) >>  2) + ((bitmaskByte & 0x30) >>  3) +  
		     	((bitmaskByte & 0x10) >>  4) + ((bitmaskByte & 0x08) << 12) +
                ((bitmaskByte & 0x0C) << 11) + ((bitmaskByte & 0x06) << 10) +
                ((bitmaskByte & 0x03) <<  9) + ((bitmaskByte & 0x01) <<  8);
          *((unsigned int *)maskLine + j) = outBytes640;
          }
        *((unsigned int *)maskLine + lineWords - 1) = 0;
     
        for (n = 0; n < lineWords * 2 - 4; n++) {
          *(imageLine + n)  = coltab640[*(dataPtr++)] & 0xC0;
          *(imageLine + n) += coltab640[*(dataPtr++)] & 0x30;
          *(imageLine + n) += coltab640[*(dataPtr++)] & 0x0C;
          *(imageLine + n) += coltab640[*(dataPtr++)] & 0x03;
          *(imageLine + n) ^= 0xFF;	    /* Reverse color */
          *(imageLine + n) &= *(maskLine + n);
          }
        *(imageLine + n) = 0;
        j = cursorPixels + rectWidth * (line + 1) - dataPtr;
        if (j-- > 0) {
          *(imageLine + n) += coltab640[*(dataPtr++)] & 0xC0;
          if (j-- > 0) {
            *(imageLine + n) += coltab640[*(dataPtr++)] & 0x30;
            if (j-- > 0) {
              *(imageLine + n) += coltab640[*(dataPtr++)] & 0x0C;
              if (j-- > 0) {
                *(imageLine + n) += coltab640[*(dataPtr++)] & 0x03;
                }
              }
            }
          }
        *(imageLine + n) ^= 0xFF;	    /* Reverse color */
        *(imageLine + n) &= *(maskLine + n);
        *(unsigned int *)(imageLine + n + 1) = 0;
        }
      }

    HUnlock(readBufferHndl);

	if (GetCursorAdr() == oldCursor)
    	SetCursor(cursor);
    if (oldCursor)
	    free(oldCursor);

#if 0
    /***************/
    {
    unsigned char * k;
    FILE *foo = fopen("out.txt", "a");
    fprintf(foo, "Width = %u, Height = %u, Hotspot X = %u, Hotspot Y = %u:\n",
                 rectWidth, rectHeight, rectX, rectY);
    fprintf(foo, "\n");
    for (k = cursor; k < cursorImage; k++)
	    fprintf(foo, "%02X ", *k);
    for (j = 0; j < lineWords * rectHeight * 4; j++) {
	    fprintf(foo, "%02X", *(cursorImage + j));
        if ((j+1) % (lineWords * 2) == 0)
	        fprintf(foo, "\n");
        }
    for (k = cursorImage + j; k < cursorImage + j + 4; k = k + 1)
	    fprintf(foo, "%02X ", *k);
    //for (j = 0; j < bitmaskLineBytes*rectHeight; j++) {
    //    fprintf(foo, "%02X", *(bitmask + j));
    //    if ((j+1) % bitmaskLineBytes == 0)
	//        fprintf(foo, "\n");
    //    }
    fprintf(foo, "\n");
    fclose(foo);
    }
    /***************/
#endif

    displayInProgress = FALSE;
    NextRect();             /* Prepare for next rect */
    }
