.PHONY: clean all

scripts_src := $(filter-out Makefile,$(filter-out %.c,$(wildcard *)))
scripts_dst := $(patsubst %,/home/$(USER)/.bin/%,$(scripts_src))

sources := $(wildcard *.c)
binaries := $(patsubst %.c,/home/$(USER)/.bin/%,$(sources))

all: $(scripts_dst) $(binaries)

common_opts := -std=c99 -g -Wall -Wextra -Werror -O2
link_opts := -lm -lrt -lX11 -lpcap -lssl -lcrypto -lreadline

clean:
	rm $(binaries)

/home/$(USER)/.bin/%: %.c
	@echo building $@
	@gcc $(common_opts) -o $@ $< $(link_opts)

/home/$(USER)/.bin/%: %
	@echo linking $@
	@ln -s $(PWD)/$< $@
