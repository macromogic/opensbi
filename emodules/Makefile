SUBDIRS := drv_console emodule_base

all: $(SUBDIRS)
$(SUBDIRS):
	make -C $@

clean:
	@rm -rf ../build/emodules

.PHONY: all $(SUBDIRS)