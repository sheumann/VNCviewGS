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
#include "menus.h"
#include "clipboard.h"
#include "desktopsize.h"
#include "mouse.h"
#include "keyboard.h"
#include "copyrect.h"
#include "raw.h"
#include "hextile.h"

/* This prototype should be in <window.h> but is bogusly commented out there */
extern pascal void SetContentOrigin2(Word, Word, Word, GrafPortPtr) inline(0x570E,dispatcher);

void DoDesktopSize (void) {
    #define screenTooBigError   2010
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
    winHeight = WIN_HEIGHT;
    winWidth = (hRez == 640) ? WIN_WIDTH_640 : WIN_WIDTH_320;
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

    NextRect();             /* Prepare for next rect */
}

