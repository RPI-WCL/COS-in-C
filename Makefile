SUBDIRS = Common VmMon COS Node

.PHONY: all $(SUBDIRS)

all:
	-for d in $(SUBDIRS); do (cd $$d; $(MAKE) lib); done
	-for d in $(SUBDIRS); do (cd $$d; $(MAKE) entity); done

clean:
	-for d in $(SUBDIRS); do (cd $$d; $(MAKE) clean); done

$(SUBDIRS):
	if [ ! -d Lib ]; then mkdir Lib; fi
	$(MAKE) -C $@


