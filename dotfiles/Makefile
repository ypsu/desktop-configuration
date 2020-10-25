src := $(filter-out Makefile,$(sort $(wildcard *)))
dst := $(src:%=$(HOME)/.%)

.PHONY: all clean
all: $(dst)
$(HOME)/.%: %
	@echo symlinking $(notdir $@)
	@ln -s $(PWD)/$< $@

clean:
	rm $(dst)
