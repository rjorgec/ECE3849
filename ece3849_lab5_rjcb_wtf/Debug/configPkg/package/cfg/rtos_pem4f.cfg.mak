# invoke SourceDir generated makefile for rtos.pem4f
rtos.pem4f: .libraries,rtos.pem4f
.libraries,rtos.pem4f: package/cfg/rtos_pem4f.xdl
	$(MAKE) -f C:\Users\rjcroesball\Documents\3849reprj\ece3849_lab4_rjcroesball_wtfolan_4_18_FINITO.zip_expanded\ece3849_lab4_rjcb_wtf/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\rjcroesball\Documents\3849reprj\ece3849_lab4_rjcroesball_wtfolan_4_18_FINITO.zip_expanded\ece3849_lab4_rjcb_wtf/src/makefile.libs clean

