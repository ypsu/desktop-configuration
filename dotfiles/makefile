src := $(filter-out makefile regen %.gen,$(sort $(wildcard *)))
dst := $(src:%=$(HOME)/.%)

.PHONY: all regen clean
all: $(dst) regen
$(HOME)/.%: %
	@echo symlinking $(notdir $@)
	@ln -s $(PWD)/$< $@

regen:
	@./regen

clean:
	rm $(dst)
