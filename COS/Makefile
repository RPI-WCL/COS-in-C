CC = gcc
CFLAGS = -g -fPIC
LDFLAGS = -L../Lib
INCLUDES = -I../Common -I../NodeManager -I. 
LIBS =
TARGET = libCosManager.so CosManager 

all: $(TARGET) 

lib: libCosManager.so

entity: CosManager

libCosManager.so: CosManager.o cos_msg.o
	$(CC) -shared -o $@ CosManager.o cos_msg.o $(LDFLAGS) -lCommon
	mv $@ ../Lib

CosManager: CosManager.o cos_main.o cos_msg.o
	$(CC) -o $@ CosManager.o cos_main.o cos_msg.o $(LDFLAGS) -lCommon -lNodeManager

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) *.o

