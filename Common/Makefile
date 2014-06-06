CC = gcc
CFLAGS = -g -fPIC
LDFLAGS =
INCLUDES = -I.
TARGET = 

OBJS = Socket.o ParseMsg.o

lib: libCommon.so

libCommon.so: $(OBJS) 
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
	mv $@ ../Lib

entity: 

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -c $< -o $*.o

.PHONY: clean
clean:
	rm -f $(OBJS)

Socket.o : Socket.h DebugPrint.h
ParseMsg.o : ParseMsg.h DebugPrint.h

