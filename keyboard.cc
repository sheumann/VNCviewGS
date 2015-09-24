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
    unsigned long key = myEvent.message & 0x0000007F;

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

    if (key == 0x7f)
        key = 0xFF08;   /* Delete -> BackSpace */

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
            case 0x1B:  key = 0xFF1B;   break;  /* Escape */
            case 0x09:  key = 0xFF09;   break;  /* Tab */
            case 0x0D:  key = 0xFF0D;   break;  /* Return / Enter */
            case 0x08:  key = 0xFF51;   break;  /* Left arrow */
            case 0x0B:  key = 0xFF52;   break;  /* Up arrow */
            case 0x15:  key = 0xFF53;   break;  /* Right arrow */
            case 0x0A:  key = 0xFF54;   break;  /* Down arrow */
            case 0x18:  key = 0xFF0B;   break;  /* Clear / NumLock -> Clear */
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

