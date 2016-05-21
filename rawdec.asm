	mcopy	rawdec.macros
	gen	on
	case	on

* Pointer to direct page space for use by these routines
dpPtr	data
	ds	4
	end

* DP locations
destOfst	gequ	0	offset into destBuf to start at
endOfst	gequ	2	offset into destBuf to end before
loop1End	gequ	4	offset into destBuf to end loop 1
oldDB	gequ	6	data bank on entry
oldDP	gequ	8	direct page on entry

* Generates 640-mode SHR pixels [destBuf+startOffset, destBuf+endOffset)
* from raw pixels starting at lineDataPtr.  Returns next lineDataPtr value.
*
* First loop is unrolled to minimize index calculation overhead,
* and runs until less that 8 iterations (output bytes) remain.
* Second loop generates the remaining bytes.
*
* unsigned char * rawDecode640(unsigned startOffset, unsigned endOffset,
*                              unsigned char *lineDataPtr);
rawDecode640	start rawDec640
	rawdec 640,8
	end

* Same for 320-mode
rawDecode320	start rawDec320
	rawdec 320,8
	end
