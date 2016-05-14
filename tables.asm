	case on

* Color tables
* 64k each, each in its own segment

BCT640A	data	COLTAB640A
	ds	$10000
	end

BCT640B	data	COLTAB640B
	ds	$10000
	end

BCT320	data	COLTAB320
	ds	$10000
	end

* Pointers to color tables, used for accessing them from C
* These must be in the blank load segment so they can be accessed as C globals
* (in the small memory model).

bigcoltab640a	data
	dc	i4'BCT640A'
	end

bigcoltab640b	data
	dc	i4'BCT640B'
	end

bigcoltab320	data
	dc	i4'BCT320'
	end

* Buffer for SHR pixel data generated during raw decoding
* Must be big enough to hold at least the full window area full of pixels.

destBuf	data
	ds	$8001
	end
