/********************************************************************
* vncsession.h - functions for establishing connection to VNC server
*                and communicating with it
********************************************************************/

#include <types.h>

#define RFBVERSIONSTR "RFB 003.003\n"
#define RFBMAJORVERSIONSTR "003"

#define SwapBytes2(x)   (((unsigned int)x << 8) |  ((unsigned int)x >> 8))
#define SwapBytes4(x)   (((unsigned long)x << 24) | \
                        ((unsigned long)x >> 24) | \
                        (((unsigned long)x & 0x0000FF00) << 8) | \
                        (((unsigned long)x & 0x00FF0000) >> 8))

extern GrafPortPtr connectStatusWindowPtr;

extern void ** readBufferHndl;

extern unsigned int hostIpid;

extern void DisplayConnectStatus(char *, BOOLEAN);

extern void DoConnect (void);

#define DoneWithReadBuffer() do         \
    if (readBufferHndl) {               \
        DisposeHandle(readBufferHndl);  \
        readBufferHndl = NULL;          \
    } while (0)                         \

extern BOOLEAN DoReadTCP (unsigned long);
extern BOOLEAN DoWaitingReadTCP(unsigned long);
extern void CloseTCPConnection (void);
