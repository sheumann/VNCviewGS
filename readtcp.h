/* Ptr to data read by last DoReadTCP call. */
extern unsigned char *readBufferPtr;

/* Used internally by TCP read routines.
 * Shouldn't be accessed directly otherwise. */
extern void ** readBufferHndl;

#define DoneWithReadBuffer() do         \
    if (readBufferHndl) {               \
        DisposeHandle(readBufferHndl);  \
        readBufferHndl = NULL;          \
    } while (0)                         \

extern BOOLEAN DoReadTCP (unsigned long);
extern BOOLEAN DoWaitingReadTCP(unsigned long);
extern unsigned DoReadMultipleTCP(unsigned recLen, unsigned maxN);
