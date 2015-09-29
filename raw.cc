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

/* Data on state of raw rectangle drawing routines */
unsigned int lineBytes;         /* Number of bytes in a line of GS pixels */
unsigned long pixels;

unsigned int drawingLine;       /* Line to be drawn while displaying */
static BOOLEAN extraByteAdvance;

unsigned char *destPtr;

/* Ends drawing of a raw rectangle when it is complete or aborted
 * because the rectangle is not visible.
 */
void StopRawDrawing (void) {
    HUnlock(readBufferHndl);
    free(srcLocInfo.ptrToPixImage);     /* Allocated as destPtr */

    displayInProgress = FALSE;

    NextRect();                                     /* Prepare for next rect */
}

#pragma optimize 95     /* To work around an ORCA/C optimizer bug */

/* Draw one or more lines from a raw rectangle
 */
void RawDraw (void) {      
    unsigned int i;         /* Loop indices */
    unsigned char *dataPtr;
    unsigned char *lineDataPtr, *initialLineDataPtr;
    unsigned char *finalDestPtr;
    static EventRecord unusedEventRec;

    /* For use with GetContentOrigin() */
    unsigned long contentOrigin;
    Point * contentOriginPtr = (void *) &contentOrigin;

    SetPort(vncWindow);                         /* Drawing in VNC window */
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
    
            if (drawingLine >= rectHeight) {    /* Sanity check */
                StopRawDrawing();
                return;
            }
        }
        else if (rectY + rectHeight - 1 > contentOriginPtr->v + winHeight)
            rectHeight = contentOriginPtr->v + winHeight - rectY + 1;

        checkBounds = FALSE;
    }

    lineDataPtr = dataPtr + (unsigned long) drawingLine * rectWidth;
            
    do {    /* We short-circuit back to here if there are no events pending */

        finalDestPtr = destPtr + lineBytes - 1;
        if (hRez == 640) {
            initialLineDataPtr = lineDataPtr;
            while (destPtr + 7 < finalDestPtr) {    /* Unrolled loop */
                *(unsigned*)destPtr     = 
                        *(unsigned*)(bigcoltab640a + *(unsigned*)lineDataPtr)
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[1]);
                *(unsigned*)(destPtr+1) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[2])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[3]);
                *(unsigned*)(destPtr+2) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[4])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[5]);
                *(unsigned*)(destPtr+3) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[6])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[7]);
                *(unsigned*)(destPtr+4) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[8])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[9]);
                *(unsigned*)(destPtr+5) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[10])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[11]);
                *(unsigned*)(destPtr+6) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[12])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[13]);
                *           (destPtr+7) = 
                        *(unsigned*)(bigcoltab640a + ((unsigned*)lineDataPtr)[14])
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[15]);
                destPtr += 8;
                lineDataPtr += 32;
            }
            while (destPtr < finalDestPtr) {
                *(destPtr++) = 
                        *(unsigned*)(bigcoltab640a + *(unsigned*)lineDataPtr)
                      + *(unsigned*)(bigcoltab640b + ((unsigned*)lineDataPtr)[1]);
                lineDataPtr += 4;
            }
            /* Final byte to produce */
            *destPtr      = pixTransTbl[*(lineDataPtr++)] & 0xC0;
            for (i = lineDataPtr - initialLineDataPtr; i < rectWidth; i++)
                switch (i & 0x03) {
                    case 0x01:          /* pixels 1, 5, 9, ... */
                        *destPtr     += pixTransTbl[*(lineDataPtr++)] & 0x30;
                        break;
                    case 0x02:          /* pixels 2, 6, 10, ... */
                        *destPtr     += pixTransTbl[*(lineDataPtr++)] & 0x0C;
                        break;
                    case 0x03:          /* pixels 3, 7, 11, ... */
                        *destPtr     += pixTransTbl[*(lineDataPtr++)] & 0x03;
                }
            destPtr++;
        }
        else {          /* 320 mode */
            while (destPtr + 7 < finalDestPtr) {    /* Unrolled loop */
            	unsigned inPixelsA, inPixelsB, outPixels;
                *(unsigned*)destPtr     = 
                        outPixels = *(unsigned*)(bigcoltab320 + (inPixelsA = *(unsigned*)lineDataPtr));
                *(unsigned*)(destPtr+1) = 
                        (inPixelsA ^ (inPixelsB = ((unsigned*)lineDataPtr)[1])) == 0 ? outPixels :
                        (outPixels = *(unsigned*)(bigcoltab320 + inPixelsB));
                *(unsigned*)(destPtr+2) = 
                        (inPixelsB ^ (inPixelsA = ((unsigned*)lineDataPtr)[2])) == 0 ? outPixels :
                        (outPixels = *(unsigned*)(bigcoltab320 + inPixelsA));
                *(unsigned*)(destPtr+3) = 
                        (inPixelsA ^ (inPixelsB = ((unsigned*)lineDataPtr)[3])) == 0 ? outPixels :
                        (outPixels = *(unsigned*)(bigcoltab320 + inPixelsB));
                *(unsigned*)(destPtr+4) = 
                        (inPixelsB ^ (inPixelsA = ((unsigned*)lineDataPtr)[4])) == 0 ? outPixels :
                        (outPixels = *(unsigned*)(bigcoltab320 + inPixelsA));
                *(unsigned*)(destPtr+5) = 
                        (inPixelsA ^ (inPixelsB = ((unsigned*)lineDataPtr)[5])) == 0 ? outPixels :
                        (outPixels = *(unsigned*)(bigcoltab320 + inPixelsB));
                *(unsigned*)(destPtr+6) = 
                        (inPixelsB ^ (inPixelsA = ((unsigned*)lineDataPtr)[6])) == 0 ? outPixels :
                        (outPixels = *(unsigned*)(bigcoltab320 + inPixelsA));
                *(destPtr+7) = 
                        (inPixelsA ^ (inPixelsB = ((unsigned*)lineDataPtr)[7])) == 0 ? outPixels :
                        *(unsigned*)(bigcoltab320 + inPixelsB);
                destPtr += 8;
                lineDataPtr += 16;
            }
            while (destPtr < finalDestPtr) {
                *(destPtr++) = 
                        *(unsigned*)(bigcoltab320 + *(unsigned*)lineDataPtr);
                lineDataPtr += 2;
            }
            /* Final byte to produce */
            *destPtr      = pixTransTbl[*(lineDataPtr++)] & 0xF0;
            if (extraByteAdvance)
                destPtr++;  /* Not ending on byte boundary - update index */
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

        SystemTask();   /* Let periodic Desk Accesories do their things */
        TCPIPPoll();    /* Let Marinetti keep processing data */

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
        if (rectWidth & 0x03)           /* Width not an exact multiple of 4 */
            lineBytes = rectWidth/4 + 1;
        else                            /* Width is a multiple of 4 */
            lineBytes = rectWidth/4;
    }
    else {  /* 320 mode */
        if (rectWidth & 0x01)           /* Width not an exact multiple of 2 */
            lineBytes = rectWidth/2 + 1;
        else                            /* Width is a multiple of 2 */
            lineBytes = rectWidth/2;
    }

    destPtr = calloc(lineBytes, 1);
    if (!destPtr) {                     /* Couldn't allocate memory */
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
            case 0x00:  srcLocInfo.boundsRect.h2 = rectWidth;       break;
            case 0x01:  srcLocInfo.boundsRect.h2 = rectWidth+3;     break;
            case 0x02:  srcLocInfo.boundsRect.h2 = rectWidth+2;     break;
            case 0x03:  srcLocInfo.boundsRect.h2 = rectWidth+1;
        }
    }
    else {
        switch (rectWidth & 0x01) {
            case 0x00:  srcLocInfo.boundsRect.h2 = rectWidth;       break;
            case 0x01:  srcLocInfo.boundsRect.h2 = rectWidth+1;
        }
    }   

    /* Don't include padding in the area we will actually copy over */
    srcRect.h2 = rectWidth;
    srcRect.v1 = 0;
    srcRect.v2 = 1;

    HLock(readBufferHndl);
    dataPtr = (unsigned char *) *readBufferHndl;
    SetPort(vncWindow);                         /* Drawing in VNC window */

    if (hRez == 640)
        for (i = 0; i < rectWidth; /* i is incremented in loop */) {
            switch (i & 0x03) {
                case 0x00:          /* pixels 0, 4, 8, ... */
                    *destPtr      = pixTransTbl[dataPtr[i++]] & 0xC0;
                    break;
                case 0x01:          /* pixels 1, 5, 9, ... */
                    *destPtr     += pixTransTbl[dataPtr[i++]] & 0x30;
                    break;
                case 0x02:          /* pixels 2, 6, 10, ... */
                    *destPtr     += pixTransTbl[dataPtr[i++]] & 0x0C;
                    break;
                case 0x03:          /* pixels 3, 7, 11, ... */
                    *(destPtr++) += pixTransTbl[dataPtr[i++]] & 0x03;
            }
        }
    else            /* 320 mode */
        for (i = 0; i < rectWidth; /* i is incremented in loop */) {
            if ((i & 0x01) == 0)            /* pixels 0, 2, 4, ... */
                *destPtr      = pixTransTbl[dataPtr[i++]] & 0xF0;
            else {                  /* pixels 1, 3, 5, ... */
                *(destPtr++) += pixTransTbl[dataPtr[i++]] & 0x0F;
            }
        }

    HUnlock(readBufferHndl);
    contentOrigin = GetContentOrigin(vncWindow);
    PPToPort(&srcLocInfo, &srcRect, rectX - contentOriginPtr->h,
            rectY - contentOriginPtr->v, modeCopy);
    free(srcLocInfo.ptrToPixImage);     /* Allocated as destPtr */

    TCPIPPoll();

    rectHeight--;   /* One less line left to draw */
    rectY++;        /* Rest of rect starts one line below this */
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
            RawDrawLine();          /* Some data ready - draw first line */
        return;                         /* Not ready yet; wait */
    }

    /* Here if data is ready to be processed */

    if (hRez == 640) {
        if (rectWidth & 0x03) {         /* Width not an exact multiple of 4 */
            lineBytes = rectWidth/4 + 1;
            extraByteAdvance = TRUE;
        }
        else {                          /* Width is a multiple of 4 */
            lineBytes = rectWidth/4;
            extraByteAdvance = FALSE;
        }
    }
    else {  /* 320 mode */
        if (rectWidth & 0x01) {         /* Width not an exact multiple of 2 */
            lineBytes = rectWidth/2 + 1;
            extraByteAdvance  = TRUE;
        }
        else {                          /* Width is a multiple of 2 */
            lineBytes = rectWidth/2;
            extraByteAdvance = FALSE;
        }
    }

    bufferLength = lineBytes * rectHeight;
    destPtr = calloc(bufferLength, 1);
    if (!destPtr) {                     /* Couldn't allocate memory */
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
            case 0x00:  srcLocInfo.boundsRect.h2 = rectWidth;       break;
            case 0x01:  srcLocInfo.boundsRect.h2 = rectWidth+3;     break;
            case 0x02:  srcLocInfo.boundsRect.h2 = rectWidth+2;     break;
            case 0x03:  srcLocInfo.boundsRect.h2 = rectWidth+1;
        }
    }
    else {
        switch (rectWidth & 0x01) {
            case 0x00:  srcLocInfo.boundsRect.h2 = rectWidth;       break;
            case 0x01:  srcLocInfo.boundsRect.h2 = rectWidth+1;
        }
    }   

    /* Don't include padding in the area we will actually copy over */
    srcRect.h2 = rectWidth;
    srcRect.v1 = 0;

    displayInProgress = TRUE;
    drawingLine = 0;            /* Drawing first line of rect */
    checkBounds = TRUE;         /* Flag to check bounds when drawing 1st line */
    HLock(readBufferHndl);      /* Lock handle just once for efficiency */
}
