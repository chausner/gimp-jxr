export CFLAGS = -w -O -I/usr/include/jxrlib -D__ANSI__ -DDISABLE_PERF_MEASUREMENT src/load.c src/save.c src/utils.c
export LIBS = -ljxrglue -ljpegxr

file-jxr: src/*
	gimptool-2.0 --build src/file-jxr.c

install:
	gimptool-2.0 --install-bin file-jxr

uninstall:
	gimptool-2.0 --uninstall-bin file-jxr

clean:
	rm -f file-jxr