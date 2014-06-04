subdirs := Common VmMon COS Node
.PHONY: all $(subdirs)

all: $(subdirs)

clean:
	-for d in $(subdirs); do (cd $$d; $(MAKE) clean); done

$(subdirs):
	if [ ! -d Lib ]; then mkdir Lib; fi
	$(MAKE) -C $@

