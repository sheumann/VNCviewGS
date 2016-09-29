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
#include "keyboard.h"

static unsigned long macRomanToKeysym[128] = {
    /* 80 - 87 */  0xc4, 0xc5, 0xc7, 0xc9, 0xd1, 0xd6, 0xdc, 0xe1,
    /* 88 - 8f */  0xe0, 0xe2, 0xe4, 0xe3, 0xe5, 0xe7, 0xe9, 0xe8,
    /* 90 - 97 */  0xea, 0xeb, 0xed, 0xec, 0xee, 0xef, 0xf1, 0xf3,
    /* 98 - 9f */  0xf2, 0xf4, 0xf6, 0xf5, 0xfa, 0xf9, 0xfb, 0xfc,
    /* a0 - a7 */  0x0af1, 0xb0, 0xa2, 0xa3, 0xa7, 0x01002022, 0xb6, 0xdf,
    /* a8 - af */  0xae, 0xa9, 0x0ac9, 0xb4, 0xa8, 0x08bd, 0xc6, 0xd8,
    /* b0 - b7 */  0x08c2, 0xb1, 0x08bc, 0x08be, 0xa5, 0xb5, 0x08ef, 0x01002211,
    /* b8 - bf */  0x0100220F, 0x07f0, 0x08bf, 0xaa, 0xba, 0x07d9, 0xe6, 0xf8,
    /* c0 - c7 */  0xbf, 0xa1, 0xac, 0x08d6, 0x08f6, 0x01002248, 0x01002206, 0xab,
    /* c8 - cf */  0xbb, 0x0aae, 0xa0, 0xc0, 0xc3, 0xd5, 0x13bc, 0x13bd,
    /* d0 - d7 */  0x0aaa, 0x0aa9, 0x0ad2, 0x0ad3, 0x0ad0, 0x0ad1, 0xf7, 0x010025CA,
    /* d8 - df */  0xff, 0x13be, 0x01002044, 0xa4, 0x01002039, 0x0100203A, 0x0100FB01, 0x0100FB02,
    /* e0 - e7 */  0x0af2, 0xb7, 0x0afd, 0x0afe, 0x0ad5, 0xc2, 0xca, 0xc1,
    /* e8 - ef */  0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0xd3, 0xd4,
    /* f0 - f7 */  0x0100F8FF, 0xd2, 0xda, 0xdb, 0xd9, 0x02b9, 0x010002C6, 0x010002DC,
    /* f8 - ff */  0xaf, 0x01a2, 0x01ff, 0x010002DA, 0xb8, 0x01bd, 0x01b2, 0x01b7,
};

/* Send a KeyEvent message to the server
 */
void SendKeyEvent (BOOLEAN keyDownFlag, unsigned long key)
{
    struct KeyEvent {
        unsigned char messageType;
        unsigned char keyDownFlag;
        unsigned int  padding;
        unsigned long key;
    } keyEvent = {  4 /* Message Type 4 */,
                    0,
                    0  /* Zero the padding */
    };
                        
    keyEvent.keyDownFlag = !!keyDownFlag;
    keyEvent.key = SwapBytes4(key);
    TCPIPWriteTCP(hostIpid, &keyEvent.messageType, sizeof(keyEvent),
                    TRUE, FALSE);
    /* No error checking here -- Can't respond to one usefully. */  
}


/* Process a key down event and send it on to the server. */
void ProcessKeyEvent (void)
{
    unsigned long key = myEvent.message & 0x000000FF;

    if (viewOnlyMode)
        return;

    /* Deal with extended keys that are mapped as keypad keys */
    if (myEvent.modifiers & keyPad) {
        switch (key) {
            case 0x7A:  key = 0xFFBE;   break;  /* F1 */
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
            case 0x69:  key = 0xFF15;   break;  /* F13 / PrintScr -> SysRq */
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

    if (key == 0x7f) {
        key = 0xFF08;   /* Delete -> BackSpace */
    } else if (key < 0x20) {
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
            case 0x1B:  key = 0xFF1B;   break;  /* Escape */
            case 0x09:  key = 0xFF09;   break;  /* Tab */
            case 0x0D:  key = 0xFF0D;   break;  /* Return / Enter */
            case 0x08:  key = 0xFF51;   break;  /* Left arrow */
            case 0x0B:  key = 0xFF52;   break;  /* Up arrow */
            case 0x15:  key = 0xFF53;   break;  /* Right arrow */
            case 0x0A:  key = 0xFF54;   break;  /* Down arrow */
            case 0x18:  key = 0xFF0B;   break;  /* Clear / NumLock -> Clear */
        }
    } else if (key & 0x80) {
        key = macRomanToKeysym[key & 0x7f];
    }

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

    /* Apple key is sent as "alt" */
    /* The opposite mapping of OA (Command) and Option might seem more logical,
     * but this matches the de facto standard of Mac VNC implementations. */
    if ((modifiers & appleKey) != (oldModifiers & appleKey))
        SendKeyEvent(modifiers & appleKey, 0xFFE9);

    if ((modifiers & shiftKey) != (oldModifiers & shiftKey))
        SendKeyEvent(modifiers & shiftKey, 0xFFE1);

    /* Option key is sent as "meta" */
    if ((modifiers & optionKey) != (oldModifiers & optionKey))
        SendKeyEvent(modifiers & optionKey, 0xFFE7);

    if ((modifiers & controlKey) != (oldModifiers & controlKey))
        SendKeyEvent(modifiers & controlKey, 0xFFE3);

    oldModifiers = modifiers;
}

