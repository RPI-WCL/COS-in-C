CC = gcc
CFLAGS = -g -fPIC
LDFLAGS = -L../Lib
INCLUDES = -I../Common -I../COS -I.
LIBS = -lCommon -lCosManager -lVmMonitor
TARGET = libNodeManager.so NodeManager

all: $(TARGET) 

lib: libNodeManager.so

entity: NodeManager

libNodeManager.so: NodeManager.o node_msg.o
	$(CC) -shared -o $@ NodeManager.o node_msg.o $(LDFLAGS) -lCommon
	mv $@ ../Lib

NodeManager: NodeManager.o node_main.o node_msg.o
	$(CC) -o $@ NodeManager.o node_main.o node_msg.o -lrt $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) *.o

