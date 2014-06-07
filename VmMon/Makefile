CC = gcc
CFLAGS = -g -fPIC
LDFLAGS = -L../Lib
INCLUDES = -I../Common -I../Node -I.
LIBS = -lCommon -lNodeManager -lVmMonitor -lCosManager
TARGET = libVmMonitor.so VmMonitor t_highcpu t_lowcpu

all: $(TARGET) 

lib: libVmMonitor.so

entity: VmMonitor

test:  t_highcpu t_lowcpu

libVmMonitor.so: VmMonitor.o vmmon_msg.o
	$(CC) -shared -o $@ VmMonitor.o vmmon_msg.o $(LDFLAGS) -lCommon
	mv $@ ../Lib

VmMonitor: VmMonitor.o vmmon_msg.o vmmon_main.o
	$(CC) -o $@ VmMonitor.o vmmon_msg.o vmmon_main.o $(INCLUDES) -lrt $(LDFLAGS) $(LIBS)

t_highcpu: t_highcpu.c
	$(CC) -g -o $@ t_highcpu.c $(INCLUDES) $(LDFLAGS) $(LIBS)

t_lowcpu: t_lowcpu.c
	$(CC) -g -o $@ t_lowcpu.c $(INCLUDES) $(LDFLAGS) $(LIBS)

t_startvm: t_startvm.c
	$(CC) -g -o $@ t_startvm.c $(INCLUDES) $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $*.o

.PHONY: clean
clean:
	rm -f $(TARGET) *.o

