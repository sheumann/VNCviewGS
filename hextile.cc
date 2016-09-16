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
#include "hextile.h"

static unsigned int hexXTiles, hexYTiles;  /* For in-process hextile processing */
static unsigned int hexXTileNum, hexYTileNum;
static unsigned int hexTileWidth, hexTileHeight;
static unsigned char hexBackground, hexForeground;

static BOOLEAN extraByteAdvance;

/* Used in Hextile encoding */
#define Raw                 0x01
#define BackgroundSpecified 0x02
#define ForegroundSpecified 0x04
#define AnySubrects         0x08
#define SubrectsColoured    0x10
    
#define hexWaitingForSubencoding        1
#define hexWaitingForMoreInfo           2
#define hexWaitingForSubrect            4
#define hexWaitingForRawData            8

static void HexNextTile (void) {
    hexXTileNum++;
    if (hexXTileNum == hexXTiles) {
        hexYTileNum++;
        if (hexYTileNum == hexYTiles) {     /* Done with this Hextile rect */
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

static void HexRawDraw (Origin contentOrigin, int rectWidth, int rectHeight) {      
    unsigned int i, j;          /* Loop indices */
    unsigned int n = 0;
    unsigned char *dataPtr;
    unsigned char pixels[128];

    static Rect srcRect = {0,0,0,0};

    dataPtr = readBufferPtr;

    if ((hRez==640 && (rectWidth & 0x03)) || (hRez==320 && (rectWidth & 0x01)))
            extraByteAdvance = TRUE;
    else
            extraByteAdvance = FALSE;

    for (j = 0; j < rectHeight; j++) {
        for (i = 0; i < rectWidth; i++) {
            if (hRez == 640) {
                switch (i & 0x03) {
                    case 0x00:          /* pixels 0, 4, 8, ... */
                        pixels[n] = pixTransTbl[ *(dataPtr +
                                (unsigned long) j*rectWidth + i)
                                ] & 0xC0;
                        break;
                    case 0x01:          /* pixels 1, 5, 9, ... */
                        pixels[n] += pixTransTbl[ *(dataPtr +
                                (unsigned long) j*rectWidth + i)
                                ] & 0x30;
                        break;
                    case 0x02:          /* pixels 2, 6, 10, ... */
                        pixels[n] += pixTransTbl[ *(dataPtr +
                                (unsigned long) j*rectWidth + i)
                                ] & 0x0C;
                        break;
                    case 0x03:          /* pixels 3, 7, 11, ... */
                        pixels[n] += pixTransTbl[ *(dataPtr +
                                (unsigned long) j*rectWidth + i)
                                ] & 0x03;
                        n++;
                } /* switch */
            } /* if */
            else {          /* 320 mode */
                switch(i & 0x01) {
                    case 0x00:      /* pixels 0, 2, 4, ... */
                        pixels[n] = pixTransTbl[ *(dataPtr +
                                (unsigned long) j*rectWidth + i)
                                ] & 0xF0;
                           break;
                    case 0x01:      /* pixels 1, 3, 5, ... */
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
            case 0x00:  srcLocInfo.boundsRect.h2 = rectWidth;
                        srcLocInfo.width = rectWidth/4;             break;
            case 0x01:  srcLocInfo.boundsRect.h2 = rectWidth+3; 
                        srcLocInfo.width = rectWidth/4 + 1;         break;
            case 0x02:  srcLocInfo.boundsRect.h2 = rectWidth+2;     
                        srcLocInfo.width = rectWidth/4 + 1;         break;
            case 0x03:  srcLocInfo.boundsRect.h2 = rectWidth+1;
                        srcLocInfo.width = rectWidth/4 + 1;
        }
    }
    else { /* hRez == 320 */
        switch (rectWidth & 0x01) {
            case 0x00:  srcLocInfo.boundsRect.h2 = rectWidth;
                        srcLocInfo.width = rectWidth/2;             break;
            case 0x01:  srcLocInfo.boundsRect.h2 = rectWidth+1;
                        srcLocInfo.width = rectWidth/2 + 1;
        }
    }   

    srcRect.v2 = hexTileHeight;                                     
    srcRect.h2 = hexTileWidth;                                      

    PPToPort(&srcLocInfo, &srcRect,                             
        rectX + hexXTileNum * 16 - contentOrigin.pt.h,             
        rectY + hexYTileNum * 16 - contentOrigin.pt.v, modeCopy);
}

/* The macros below are used in HexDispatch() */
#define HexDispatch_NextTile()  do {                                \
    HexNextTile();                                                  \
    DoneWithReadBuffer();                                           \
    /* Set up for next time */                                      \
    status = hexWaitingForSubencoding;                              \
    bytesNeeded = 1;                                                \
    return;                                                         \
} while (0)

#define HexDispatch_DrawRect(color, X, Y, width, height)    do {        \
    SetSolidPenPat((color));   \
    drawingRect.h1 = rectX + hexXTileNum * 16 + (X) - contentOrigin.pt.h;   \
    drawingRect.v1 = rectY + hexYTileNum * 16 + (Y) - contentOrigin.pt.v;    \
    drawingRect.h2 = rectX + hexXTileNum * 16 + (X) + (width) - contentOrigin.pt.h; \
    drawingRect.v2 = rectY + hexYTileNum * 16 + (Y) + (height) - contentOrigin.pt.v; \
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
    Origin contentOrigin;
    int tileBytes;
    unsigned int srX, srY, srWidth, srHeight;
    Rect drawingRect;
    static unsigned char pixels[128];
    unsigned char *dataPtr;

    contentOrigin.l = GetContentOrigin(vncWindow);
    SetPort(vncWindow);

    /* If we don't have the next bit of needed data yet, return. */
    while (DoReadTCP(bytesNeeded)) {
        dataPtr = readBufferPtr;
        /* If we're here, readBufferPtr contains bytesNeeded bytes of data. */
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
                HexRawDraw(contentOrigin, hexTileWidth, hexTileHeight);
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
                else {  /* no subrects */
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
