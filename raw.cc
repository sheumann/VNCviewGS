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

#include "vncsession.h"
#include "vncview.h"
#include "vncdisplay.h"
#include "colortables.h"
#include "readtcp.h"
#include "raw.h"

#define EVENT_CHECK_INVERVAL 10  /* tick count between event checks */

/* Data on state of raw rectangle drawing routines */
static unsigned int lineBytes;   /* Number of bytes in a line of GS pixels */
static unsigned long pixels;

static unsigned int drawingLine; /* Line to be drawn while displaying */
static BOOLEAN extraByteAdvance;

static unsigned linesAvailable;

/* Bytes to skip before or after doing raw drawing */
static unsigned long preSkip, postSkip;

/* Buffer to hold all SHR pixel data from one update (defined in tables.asm).
 * Must be big enough: at least (WIN_WIDTH_640/4 + 1) * WIN_HEIGHT. */
#define DESTBUF_SIZE 0x8001
extern unsigned char destBuf[];

static unsigned char *destPtr;

unsigned char * rawDecode640(unsigned startOffset, unsigned endOffset,
                             unsigned char *lineDataPtr);
unsigned char * rawDecode320(unsigned startOffset, unsigned endOffset,
                             unsigned char *lineDataPtr);


/* Ends drawing of a raw rectangle when it is complete or aborted
 * because the rectangle is not visible.
 */
static void StopRawDrawing (void) {
    NextRect();                                     /* Prepare for next rect */
}

#pragma optimize 95     /* To work around an ORCA/C optimizer bug */

/* Draw one or more lines from a raw rectangle
 */
void RawDraw (void) {
    static unsigned char *lineDataPtr;
    unsigned int i;         /* Loop indices */
    unsigned char *initialLineDataPtr;
    unsigned char *finalDestPtr;
    static EventRecord eventRec;
    unsigned long eventCheckTime;

    /* For use with GetContentOrigin() */
    Origin contentOrigin;

    eventCheckTime = TickCount() + EVENT_CHECK_INVERVAL;
    SetPort(vncWindow);                         /* Drawing in VNC window */

#if 0
    /* Check if what we're drawing is visible, and skip any invisible part
     * by skipping some lines or completely aborting drawing the rectangle.
     */
    if (checkBounds) {
        Rect drawingRect;

        contentOrigin.l = GetContentOrigin(vncWindow);
        drawingRect.h1 = rectX - contentOrigin.pt.h;
        drawingRect.h2 = rectX - contentOrigin.pt.h + rectWidth;
        drawingRect.v1 = rectY - contentOrigin.pt.v + drawingLine;
        drawingRect.v2 = rectY - contentOrigin.pt.v + rectHeight;

        if (!RectInRgn(&drawingRect, GetVisHandle())) {
            StopRawDrawing();
            return;
        }
        else if (rectY + drawingLine < contentOrigin.pt.v) {
            unsigned linesToSkip = contentOrigin.pt.v - rectY - drawingLine;
            preSkip = linesToSkip * rectWidth;
            destPtr += (unsigned long)lineBytes * linesToSkip;
            drawingLine = contentOrigin.pt.v - rectY;
    
            if (drawingLine >= rectHeight) {    /* Sanity check */
                StopRawDrawing();
                return;
            }
        }
        else if (rectY + rectHeight - 1 > contentOrigin.pt.v + winHeight)
            rectHeight = contentOrigin.pt.v + winHeight - rectY + 1;

        checkBounds = FALSE;
    }
#endif

    do {    /* We short-circuit back to here if there are no events pending */
        unsigned inPixelsA, inPixelsB, outPixels;

        if (linesAvailable == 0) {
            linesAvailable = DoReadMultipleTCP(rectWidth, rectHeight - drawingLine);
            if (linesAvailable) {
                lineDataPtr = readBufferPtr;
            } else {
                return;
            }
        }

        finalDestPtr = destPtr + lineBytes - 1;
        if (hRez == 640) {
            initialLineDataPtr = lineDataPtr;
            lineDataPtr = rawDecode640(destPtr-destBuf, finalDestPtr-destBuf, lineDataPtr);
            destPtr = finalDestPtr;
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
            lineDataPtr = rawDecode320(destPtr-destBuf, finalDestPtr-destBuf, lineDataPtr);
            destPtr = finalDestPtr;
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
            contentOrigin.l = GetContentOrigin(vncWindow);
            PPToPort(&srcLocInfo, &srcRect, rectX - contentOrigin.pt.h,
                rectY + srcRect.v1 - contentOrigin.pt.v, modeCopy);
            srcRect.v1 = drawingLine;
        }

        /* Check whether we're done with this rectangle */
        if (drawingLine >= rectHeight) {
            /* Draw final rect, if necessary */
            if (drawingLine > srcRect.v1) {
                srcRect.v2 = drawingLine;
                contentOrigin.l = GetContentOrigin(vncWindow);
                PPToPort(&srcLocInfo, &srcRect, rectX - contentOrigin.pt.h,
                    rectY + srcRect.v1 - contentOrigin.pt.v, modeCopy);
            }
            StopRawDrawing();
            return;
        }

        linesAvailable--;

        /* Check if there are actually any events that need to be processed.
         * If not, save time by not going through the whole event loop, but
         * instead processing the minimum necessary periodic tasks and then
         * going straight to the next line of data.
         */
        if (TickCount() >= eventCheckTime) {
            if (EventAvail(0xFFFF, &eventRec))
                return;
            SystemTask();   /* Let periodic Desk Accesories do their things */
            eventCheckTime = eventRec.when + EVENT_CHECK_INVERVAL;
        }
        TCPIPPoll();    /* Let Marinetti keep processing data */
    } while (1);
}

#pragma optimize -1

/* Process rectangle data in raw encoding and write it to screen.
 */
void DoRawRect (void) {
    pixels = (unsigned long) rectWidth * rectHeight;

    if (hRez == 640) {
        if (rectWidth & 0x03) {         /* Width not an exact multiple of 4 */
            lineBytes = rectWidth/4 + 1;
        }
        else {                          /* Width is a multiple of 4 */
            lineBytes = rectWidth/4;
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

    /* If we get an oversized update, ignore it and ask for another one.
     * Shouldn't normally happen, except possibly if there are outstanding
     * update requests for multiple screen regions due to scrolling.
     */
    if (lineBytes * rectHeight >= DESTBUF_SIZE) {
        Origin contentOrigin;

        contentOrigin.l = GetContentOrigin(vncWindow);
        SendFBUpdateRequest(FALSE, contentOrigin.pt.h, contentOrigin.pt.v,
                            winWidth, winHeight);
        skipBytes = pixels;
        StopRawDrawing();
        return;
    }
    destPtr = destBuf;

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
    linesAvailable = 0;         /* No line data read yet */
    checkBounds = TRUE;         /* Flag to check bounds when drawing 1st line */
}
