
#if 0
/* Old version of DoReadTCP with lots of obfuscated junk in it.  Don't use. */
BOOLEAN DoReadTCP (unsigned long dataLength, BOOLEAN waitForData) {
	#define buffTypePointer 0x0000
    #define buffTypeHandle 0x0001
    #define buffTypeNewHandle 0x0002

    static srBuff theSRBuff;
    static rrBuff theRRBuff;
    unsigned long remainingDataLength = 0;
    unsigned long initialTime;
    void * currentDataPtr;
    static unsigned long bytesBeforeExtraBytes = 0;	/* Only valid if  	*/
													/* extraBytes > 0 	*/
    static unsigned long extraBytes = 0;

    restart:

    /* Check if there was extra data left over from the last read	*/
    if (extraBytes > 0) {
	    HLock(readBufferHndl);
        BlockMove((char *)*readBufferHndl + bytesBeforeExtraBytes,
        			*readBufferHndl, extraBytes);
        HUnlock(readBufferHndl);
        SetHandleSize(extraBytes, readBufferHndl);

   	    if (extraBytes >= dataLength) {
	        bytesBeforeExtraBytes = dataLength;
            extraBytes = extraBytes - dataLength;
            return TRUE;
            }
	    else {
	        remainingDataLength = dataLength - extraBytes;
            theRRBuff.rrPushFlag = TRUE;
            }
        }

    /* Check if there is enough data to return.  If the waitForData flag is */
    /* set, wait up to 15 seconds for the data to arrive					*/
    initialTime = TickCount();
    do {
   	    if (TickCount() >= initialTime + 15*60) {
			readError = 1;
	        return FALSE;
            } 
        TCPIPPoll();
        if ((TCPIPStatusTCP(hostIpid, &theSRBuff)) &&
        		(theSRBuff.srRcvQueued < dataLength)) {
            readError = 2;
            return FALSE;
            }
    	if (toolerror()) {
	        readError = 3;
	    	return FALSE;
            }
    	if ((theSRBuff.srRcvQueued < dataLength) && (waitForData == FALSE)) {
	    	return FALSE;
            }
        } while ((theSRBuff.srRcvQueued < dataLength));
    
    printf("Wanted %lu bytes; %lu bytes available.\n", dataLength, theSRBuff.srRcvQueued);
    printf("Out of main loop.  Started %lu; now %lu.\n", initialTime, TickCount());

	/* Try to read the data */
    if ((remainingDataLength == 0) &&
    		(TCPIPReadTCP(hostIpid, buffTypeHandle, (Ref) readBufferHndl,
    		dataLength, &theRRBuff)) && (theRRBuff.rrBuffCount < dataLength)) {
        readError = 4;
        return FALSE;
        }
    if (toolerror()) {
	    readError = 5;
	   	return FALSE;
        }

    printf("rrBuffCount (data read) = %lu; dataLength (data requested) = %lu\n", theRRBuff.rrBuffCount, dataLength);

    /* Return successfully if the data was read */
	if (theRRBuff.rrBuffCount >= dataLength) {
	    if (theRRBuff.rrBuffCount > dataLength)  {
	        extraBytes = theRRBuff.rrBuffCount - dataLength;
            bytesBeforeExtraBytes = dataLength;
            }
	    return TRUE;
        }
    /* An ugly workaround for an apparent Marinetti bug wherein at least the
     * requested amount of data is supposedly available in its buffers, but
     * for some reason Marinetti returns less data than that in TCPIPReadTCP().
     */
#if 1
    else if ((theRRBuff.rrBuffCount > 0) && /*(extraBytes == 0) &&*/
    				(theRRBuff.rrBuffCount < dataLength)) {
        char foo[200];
        char **bar = (char **)&foo;
        sprintf(foo, "Returned:%lu Wanted:%lu Supposedly available:%lu", (unsigned long)(theRRBuff.rrBuffCount), (unsigned long)dataLength, (unsigned long)(theSRBuff.srRcvQueued));
	   	//printf("Handling extra bytes\n");
	    //extraBytes = theRRBuff.rrBuffCount;
        //bytesBeforeExtraBytes = 0;
        AlertWindow(awResource, (Pointer)&bar, 10000);
        //return FALSE;
        //goto restart;
        return TRUE;
        }
#endif
    /* This may not be necessary and should not normally be used.  It       */
    /* continues requesting data until a sufficient amount has been         */
    /* received, which may be necessary when the data stream contains push	*/
    /* flags.																*/
    else if (theRRBuff.rrPushFlag) {
	    //printf("Handling push flag in middle of data.\n");
		remainingDataLength = dataLength;
        SetHandleSize(dataLength, readBufferHndl);
        HLock(readBufferHndl);
        currentDataPtr = (*readBufferHndl) + theRRBuff.rrBuffCount;

    	while (theRRBuff.rrPushFlag && (remainingDataLength > 0)) {	      
            TCPIPPoll();

            if ((TCPIPReadTCP(hostIpid, buffTypeHandle, NULL,
        			remainingDataLength, &theRRBuff)) &&
            		(theRRBuff.rrBuffCount < dataLength)) {
				readError = 6;
        		return FALSE;
        		}
	        if (toolerror()) {
	            readError = 7;
	            return FALSE;
                }
            if (theRRBuff.rrBuffCount > 0) {
                HandToPtr(theRRBuff.rrBuffHandle, currentDataPtr,
		                (theRRBuff.rrBuffCount < remainingDataLength) ?
	                    theRRBuff.rrBuffCount : remainingDataLength);
                DisposeHandle(theRRBuff.rrBuffHandle);
                if (theRRBuff.rrBuffCount > remainingDataLength) {
	                extraBytes = theRRBuff.rrBuffCount - dataLength;
                    bytesBeforeExtraBytes = remainingDataLength;
                    theRRBuff.rrBuffCount = remainingDataLength;
                    }
                currentDataPtr += theRRBuff.rrBuffCount;
                remainingDataLength -= theRRBuff.rrBuffCount;
                if (remainingDataLength == 0) {
		            HUnlock(readBufferHndl);
                    return TRUE;
                    }
                }
	        else {
	            HUnlock(readBufferHndl);
                readError = 8;
                return FALSE;
                }
            }
		}

    HUnlock(readBufferHndl);
    readError = 9;
    return FALSE;
   	#undef buffTypeNewHandle
    }
#endif /* 0 */
