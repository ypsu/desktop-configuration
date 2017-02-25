src := $(filter-out Makefile,$(sort $(wildcard *)))
dst := $(patsubst %,$(HOME)/.%,$(src))

.PHONY: all clean
all: $(dst)
$(HOME)/.%: %
	@echo symlinking $(notdir $@)
	@ln -s $(PWD)/$< $@

clean:
	rm $(dst)
