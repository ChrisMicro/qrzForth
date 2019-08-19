CC=gcc
CFLAGS=-I.
DEPS = platform.h  qrzVm.h  streamSplitter.h  systemout.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

qrzmake: platform.o  qrzForth.o  qrzVm.o  streamSplitter.o 
	$(CC) -o qrzForth platform.c  qrzForth.c  qrzVm.c  streamSplitter.c 

clean:
	rm *.o
	rm qrzForth

