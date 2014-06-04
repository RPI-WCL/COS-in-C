CC = gcc
CFLAGS = -g -fPIC
LDFLAGS = -L../Lib
INCLUDES = -I../Common -I../Node
LIBS = -lCommon -lNodeManager
TARGET = libVmMonitor.so VmMonitor t_highcpu t_lowcpu

all: $(TARGET) 

libVmMonitor.so: VmMonitor.o vmmon_msg.o
	$(CC) -shared -o $@ VmMonitor.o vmmon_msg.o $(LDFLAGS) -lCommon
	mv $@ ../Lib

VmMonitor: VmMonitor.o vmmon_msg.o vmmon_main.o
	$(CC) -o $@ VmMonitor.o vmmon_msg.o vmmon_main.o -lrt $(LDFLAGS) $(LIBS)

t_highcpu: t_highcpu.o
	$(CC) -o $@ t_highcpu.o $(LDFLAGS) $(LIBS)

t_lowcpu: t_lowcpu.o
	$(CC) -o $@ t_lowcpu.o $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) *.o

