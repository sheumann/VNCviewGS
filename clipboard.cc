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
#include "readtcp.h"
#include "clipboard.h"

/*
 * Tables for character set conversion. The current RFB spec says
 * that the clipboard data should use the ISO 8859-1 character set.
 * In practice, several servers actually use Windows-1252 (which is
 * a superset of ISO 8859-1), and even those that don't seem to be
 * able to gracefully discard the extra characters from it. 
 * We also use Windows-1252, for compatibility with servers that
 * may send it and because it enables us to map more of the MacRoman
 * characters.
 */

static unsigned char macRomanToWindows1252[128] = {
    /* 80 - 87 */  0xc4, 0xc5, 0xc7, 0xc9, 0xd1, 0xd6, 0xdc, 0xe1,
    /* 88 - 8f */  0xe0, 0xe2, 0xe4, 0xe3, 0xe5, 0xe7, 0xe9, 0xe8,
    /* 90 - 97 */  0xea, 0xeb, 0xed, 0xec, 0xee, 0xef, 0xf1, 0xf3,
    /* 98 - 9f */  0xf2, 0xf4, 0xf6, 0xf5, 0xfa, 0xf9, 0xfb, 0xfc,
    /* a0 - a7 */  0x86, 0xb0, 0xa2, 0xa3, 0xa7, 0x95, 0xb6, 0xdf,
    /* a8 - af */  0xae, 0xa9, 0x99, 0xb4, 0xa8, 0XA6, 0xc6, 0xd8,
    /* b0 - b7 */  0XA6, 0xb1, 0XA6, 0XA6, 0xa5, 0xb5, 0XA6, 0XA6,
    /* b8 - bf */  0XA6, 0XA6, 0XA6, 0xaa, 0xba, 0XA6, 0xe6, 0xf8,
    /* c0 - c7 */  0xbf, 0xa1, 0xac, 0XA6, 0x83, 0XA6, 0XA6, 0xab,
    /* c8 - cf */  0xbb, 0x85, 0xa0, 0xc0, 0xc3, 0xd5, 0x8c, 0x9c,
    /* d0 - d7 */  0x96, 0x97, 0x93, 0x94, 0x91, 0x92, 0xf7, 0XA6,
    /* d8 - df */  0xff, 0x9f, 0XA6, 0xa4, 0x8b, 0x9b, 0XA6, 0XA6,
    /* e0 - e7 */  0x87, 0xb7, 0x82, 0x84, 0x89, 0xc2, 0xca, 0xc1,
    /* e8 - ef */  0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0xd3, 0xd4,
    /* f0 - f7 */  0XA6, 0xd2, 0xda, 0xdb, 0xd9, 0XA6, 0x88, 0x98,
    /* f8 - ff */  0xaf, 0XA6, 0XA6, 0XA6, 0xb8, 0XA6, 0XA6, 0XA6,
};

static unsigned char windows1252ToMacRoman[128] = {
    /* 80 - 87 */  0XF0, 0XF0, 0xe2, 0xc4, 0xe3, 0xc9, 0xa0, 0xe0,
    /* 88 - 8f */  0xf6, 0xe4, 0XF0, 0xdc, 0xce, 0XF0, 0XF0, 0XF0,
    /* 90 - 97 */  0XF0, 0xd4, 0xd5, 0xd2, 0xd3, 0xa5, 0xd0, 0xd1,
    /* 98 - 9f */  0xf7, 0xaa, 0XF0, 0xdd, 0xcf, 0XF0, 0XF0, 0xd9,
    /* a0 - a7 */  0xca, 0xc1, 0xa2, 0xa3, 0xdb, 0xb4, 0XF0, 0xa4,
    /* a8 - af */  0xac, 0xa9, 0xbb, 0xc7, 0xc2, 0XF0, 0xa8, 0xf8,
    /* b0 - b7 */  0xa1, 0xb1, 0XF0, 0XF0, 0xab, 0xb5, 0xa6, 0xe1,
    /* b8 - bf */  0xfc, 0XF0, 0xbc, 0xc8, 0XF0, 0XF0, 0XF0, 0xc0,
    /* c0 - c7 */  0xcb, 0xe7, 0xe5, 0xcc, 0x80, 0x81, 0xae, 0x82,
    /* c8 - cf */  0xe9, 0x83, 0xe6, 0xe8, 0xed, 0xea, 0xeb, 0xec,
    /* d0 - d7 */  0XF0, 0x84, 0xf1, 0xee, 0xef, 0xcd, 0x85, 0XF0,
    /* d8 - df */  0xaf, 0xf4, 0xf2, 0xf3, 0x86, 0XF0, 0XF0, 0xa7,
    /* e0 - e7 */  0x88, 0x87, 0x89, 0x8b, 0x8a, 0x8c, 0xbe, 0x8d,
    /* e8 - ef */  0x8f, 0x8e, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,
    /* f0 - f7 */  0XF0, 0x96, 0x98, 0x97, 0x99, 0x9b, 0x9a, 0xd6,
    /* f8 - ff */  0xbf, 0x9d, 0x9c, 0x9e, 0x9f, 0XF0, 0XF0, 0xd8,
};


static unsigned long textLen;

/* Update the Scrap Manager clipboard with new data sent from server.
 */
void DoServerCutText (void) {
    if (! DoWaitingReadTCP (3)) {   /* Read & ignore padding */
        DoClose(vncWindow);
        return;
    }
    if (! DoWaitingReadTCP (4)) {
        DoClose(vncWindow);
        return;
    }
    textLen = SwapBytes4(*(unsigned long *)readBufferPtr);
    
    /* Set up to wait for clipboard data.  Treat this like a 
     * "display in progress," although it's really not. */
    displayInProgress = TRUE;
    GetClipboard();
}

void GetClipboard (void) {
    unsigned long i;

    if (! DoReadTCP(textLen)) {
        return;
    };
    if (allowClipboardTransfers) {
        ZeroScrap();
                    
        /* Convert lf->cr and convert Windows-1252 or ISO 8859-1 charset
         * to MacRoman.  Use pointer arithmetic so we can go over 64k. */
        for (i = 0; i < textLen; i++) {
            if (*(readBufferPtr+i) == '\n') {
                *(readBufferPtr+i) = '\r';
            } else if (*(readBufferPtr+i) & 0x80) {
                *(readBufferPtr+i) =
                        windows1252ToMacRoman[*(readBufferPtr+i) & 0x7f];
            }
        }

                    /* Below function call requires <scrap.h> to be fixed */
        PutScrap(textLen, textScrap, (Pointer) readBufferPtr);
        /* Potential errors (e.g. out of memory) ignored */
        DoneWithReadBuffer();
    }
    
    displayInProgress = FALSE;
}
    
void DoSendClipboard (void) {
    static struct clientCutText {
        unsigned char messageType;
        unsigned char padding1;
        unsigned int  padding2;
        unsigned long length;
    } clientCutTextStruct = { 6 /* Message type 6 */ };

    Handle scrapHandle;
    unsigned char *scrapPtr;
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
        scrapPtr = *(unsigned char **)scrapHandle;
        /* Convert cr->lf and convert MacRoman to Windows-1252 charset.
         * Use pointer arithmetic so we can go over 64k. */
        for (i = 0; i < clientCutTextStruct.length; i++) {
            if (*(scrapPtr+i) == '\r') {
                *(scrapPtr+i) = '\n';
            } else if (*(scrapPtr+i) & 0x80) {
                *(scrapPtr+i) =
                        macRomanToWindows1252[*(scrapPtr+i) & 0x7f];
            }
        }

        TCPIPWriteTCP(hostIpid, (Pointer) *scrapHandle,
                    clientCutTextStruct.length, TRUE, FALSE);
        /* Can't handle errors usefully here */
        HUnlock(scrapHandle);

        end:
        DisposeHandle(scrapHandle);
    }
}

