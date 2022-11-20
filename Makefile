CC = gcc
CFLAGS = -g -O2
SHARED = -fPIC --shared

crypto.so: luabinding.c lcodec.c lcrc.c lmd5.c lsha1.c lsha2.c
	${CC} $(CFLAGS) $(SHARED) $^ -o $@

all: crypto.so

clean:
	rm -f crypto.so

