src := $(filter-out Makefile,$(sort $(wildcard *)))
dst := $(patsubst %,$(HOME)/.%,$(src))

.PHONY: all
all: $(dst)
$(HOME)/.%: %
	@echo symlinking $(notdir $@)
	@ln -s $(PWD)/$< $@
