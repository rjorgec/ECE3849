# invoke SourceDir generated makefile for rtos.pem4f
rtos.pem4f: .libraries,rtos.pem4f
.libraries,rtos.pem4f: package/cfg/rtos_pem4f.xdl
	$(MAKE) -f C:\Users\rjcroesball\Downloads\ece3849_lab3_april9_wtf_rjcb\ece3849_lab2_starter/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\rjcroesball\Downloads\ece3849_lab3_april9_wtf_rjcb\ece3849_lab2_starter/src/makefile.libs clean

