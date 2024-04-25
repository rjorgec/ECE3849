# invoke SourceDir generated makefile for rtos.pem4f
rtos.pem4f: .libraries,rtos.pem4f
.libraries,rtos.pem4f: package/cfg/rtos_pem4f.xdl
	$(MAKE) -f C:\Users\wtfolan\ece3849_lab5_rjcb_wtf/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\wtfolan\ece3849_lab5_rjcb_wtf/src/makefile.libs clean

