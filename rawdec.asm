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
unroll	equ	8	loop unrolling factor

	tdc
	tax
	lda	|dpPtr
	tcd		set new direct page
	stx	oldDP	save direct page on entry

	phb
	phb
	pla
	sta	oldDB	save data bank on entry

	lda	10,S
	pha		leaves extra byte: clean up later
	plb		initialize data bank=bank of lineDataPtr
	lda	8+1,S	initialize y = lineDataPtr (low 16 bits)
	tay

	pla		move return address to proper position
	sta	8-1,S
	pla
	sta	10-3,S	

	plx
	stx	destOfst	initialize x = destOfst = startOffset

	pla
	sta	endOfst	initialize endOfst = endOffset

	sec
	sbc	#unroll-1
	bcs	doLoop1	if endOffset-7 did not underflow...
	jmp	test2

doLoop1	sta	loop1End	initialize loop1End = endOffset - 7
	txa		a = startOffset
	jmp	test1

loop1	sep	#$20
	longa	off
	loopBody640 unroll
	rep	#$20
	longa	on
	tya
	clc
	adc	#unroll*4	carry must be clear
	tay
	bcs	incDB1
cont1	txa
	clc
	adc	#unroll	carry must be clear
	sta	destOfst
test1	cmp	loop1End
	bge	check2
	jmp	loop1

check2	cmp	endOfst
	bge	end
loop2	anop
	loopBody640 1
	inx
	stx	destOfst
	tya
	clc
	adc	#4	carry must be clear
	tay
	bcs	incDB2
test2	cpx	endOfst
	blt	loop2

end	phb
	plx		x = old DB (high byte of lineDataPtr)
	pei	(oldDB)
	plb		restore data bank
	plb
	lda	oldDP
	tcd		restore direct page
	tya		a = lineDataPtr (low 16 bits)
	rtl

incDB1	pea	cont1-1
	bra	incDB
incDB2	pea	test2-1
incDB	phb
	pla
	inc	A
	pha
	plb
	clc
	rts
	end
