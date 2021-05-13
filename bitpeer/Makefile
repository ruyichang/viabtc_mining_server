TARGET  := bitpeer.exe
        INCS = -I ../network -I ../utils
LIBS = -L ../utils -lutils -L ../network -lcurl -lnetwork -Wl,-Bstatic -lev -lhiredis -ljansson -lssl -lcrypto -Wl,-Bdynamic -ldl -lm -lpthread -lz

SOURCE  := $(wildcard *.c)
OBJS    := $(patsubst %.c, %.o, $(SOURCE))
CC      := gcc
CFLAGS  := -Wall -Wno-strict-aliasing -Wno-uninitialized -g -rdynamic -std=gnu99
LFLAGS  := -g -rdynamic


.PHONY : all clean install

all : $(TARGET)

clean :
	rm -rf *.d *.o $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)
.c.o :
	$(CC) $(CFLAGS) -c -o $@ $< $(INCS)

install :