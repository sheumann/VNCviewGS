#if __ORCAC__
#pragma lint -1
#pragma noroot
segment "VNCview GS";
#endif

#include <tcpip.h>
#include <memory.h>
#include <event.h>
#include <orca.h>

#include "readtcp.h"
#include "vncsession.h"

#define buffTypePointer 0x0000      /* For TCPIPReadTCP() */
#define buffTypeHandle 0x0001
#define buffTypeNewHandle 0x0002

unsigned char *readBufferPtr; /* Ptr to data read by last DoReadTCP call. */
void ** readBufferHndl;       /* User internally by TCP read routines. */

static unsigned int tcperr;

/* Read data, waiting for up to 15 seconds for the data to be ready */
BOOLEAN DoWaitingReadTCP(unsigned long dataLength) {
    unsigned long stopTime;
    BOOLEAN result = FALSE;

    stopTime = TickCount() + 15 * 60;
    do {
        result = DoReadTCP(dataLength);
    } while (result == FALSE && tcperr == tcperrOK && TickCount() < stopTime);

    return result;
}


/* Fix things when TCPIPReadTCP returns less data than it's supposed to */
static BOOLEAN ReadFixup (unsigned long requested, unsigned long returned) {
    static rrBuff theRRBuff;

    SetHandleSize(requested, readBufferHndl);
    if (toolerror())
        return FALSE;
    HLock(readBufferHndl);

    do {
        TCPIPPoll();
        if ((tcperr = TCPIPReadTCP(hostIpid, buffTypeNewHandle, NULL,
                    requested-returned, &theRRBuff)) != tcperrOK)
            return FALSE;
        if (toolerror())
            return FALSE;
        
        if (theRRBuff.rrBuffCount == 0)     /* To avoid infinite loops */
            return FALSE;

        HandToPtr(theRRBuff.rrBuffHandle, (char *)*readBufferHndl + returned,
                    theRRBuff.rrBuffCount);
        returned += theRRBuff.rrBuffCount;

        DisposeHandle(theRRBuff.rrBuffHandle);
    } while (returned < requested);

    readBufferPtr = *readBufferHndl;
    return TRUE;
}

/**********************************************************************
* DoReadTCP() - Issue TCPIPReadTCP() call w/ appropriate parameters
*   Return value = did the read succeed?
**********************************************************************/
BOOLEAN DoReadTCP (unsigned long dataLength) {
    static srBuff theSRBuff;
    static rrBuff theRRBuff;

    if (dataLength == 0)
        return TRUE;
    DoneWithReadBuffer();
    TCPIPPoll();

    if ((tcperr = TCPIPStatusTCP(hostIpid, &theSRBuff)) != tcperrOK)
        return FALSE;
    if (toolerror())
        return FALSE;

    if (theSRBuff.srRcvQueued < dataLength)
        return FALSE;

    if ((tcperr = TCPIPReadTCP(hostIpid, buffTypeNewHandle, NULL,
                        dataLength, &theRRBuff)) != tcperrOK)
        return FALSE;
    if (toolerror())
        return FALSE;
    if (theRRBuff.rrBuffCount == 0)
        return FALSE;

    readBufferHndl = theRRBuff.rrBuffHandle;

    if (theRRBuff.rrBuffCount != dataLength)
        return ReadFixup(dataLength, theRRBuff.rrBuffCount);

    HLock(readBufferHndl);
    readBufferPtr = *readBufferHndl;
    return TRUE;
}

/**********************************************************************
* DoReadMultipleTCP() - Read the largest available multiple of recLen bytes,
*   up to a maximum multiple of maxN.
*   Return value = the multiple n (meaning n * len bytes have been read)
**********************************************************************/
unsigned DoReadMultipleTCP(unsigned recLen, unsigned maxN) {
    static srBuff theSRBuff;
    unsigned long n, totalSize;
    
    TCPIPPoll();

    if ((tcperr = TCPIPStatusTCP(hostIpid, &theSRBuff)) != tcperrOK)
        return 0;
    if (toolerror())
        return 0;

    n = theSRBuff.srRcvQueued / recLen;
    if (n > maxN)
        n = maxN;
    
    if (n && DoReadTCP(recLen * n))
        return n;

    return 0;
}
