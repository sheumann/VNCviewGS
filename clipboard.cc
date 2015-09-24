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

/* Update the Scrap Manager clipboard with new data sent from server.
 */
void DoServerCutText (void) {
    unsigned long textLen;
    unsigned long i;

    if (! DoWaitingReadTCP (3)) {   /* Read & ignore padding */
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
            goto end;                       /* abort if error */
        if (TCPIPWriteTCP(hostIpid, &clientCutTextStruct.messageType,
                    sizeof(clientCutTextStruct), FALSE, FALSE))
            goto end;                       /* abort if error */
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

