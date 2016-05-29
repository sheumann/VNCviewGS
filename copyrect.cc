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

void DoCopyRect (void) {
    /* For use with GetContentOrigin() */
    unsigned long contentOrigin;
    Point * contentOriginPtr = (void *) &contentOrigin;
    
    Rect srcRect;
    unsigned int *dataPtr;              /* Pointer to TCP data that was read */

    //printf("Processing CopyRect rectangle\n");

    if (! DoReadTCP ((unsigned long) 4))
        return;                                     /* Not ready yet; wait */

    contentOrigin = GetContentOrigin(vncWindow);

    dataPtr = (unsigned int *) ((char *) (*readBufferHndl));
    srcRect.h1 = SwapBytes2(dataPtr[0]) - contentOriginPtr->h;
    srcRect.v1 = SwapBytes2(dataPtr[1]) - contentOriginPtr->v;

    srcRect.h2 = srcRect.h1 + rectWidth;
    srcRect.v2 = srcRect.v1 + rectHeight;

    /* Check that the source rect is actually visible; if not, ask the server
       to send the update using some other encoding.
     */
    if (!RectInRgn(&srcRect, GetVisHandle())) {
        SendFBUpdateRequest(FALSE, rectX, rectY, rectWidth, rectHeight);
        goto done;
    }

    /* We can use the window pointer as a LocInfo pointer because it starts
     * with a grafPort structure, which in turn starts with a LocInfo structure.
     */
    PPToPort((struct LocInfo *) vncWindow, &srcRect,
            rectX - contentOriginPtr->h, rectY - contentOriginPtr->v, modeCopy);

done:
    NextRect();                                     /* Prepare for next rect */
}

