CC = gcc
CFLAGS = -g -fPIC
LDFLAGS =
JNI_INCLUDES =
INCLUDES =
LIBS =
TARGET = libCommon.so
OBJS = Socket.o ParseMsg.o

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
	mv $(TARGET) ../Lib

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)

Socket.o : Socket.h DebugPrint.h
ParseMsg.o : ParseMsg.h DebugPrint.h

