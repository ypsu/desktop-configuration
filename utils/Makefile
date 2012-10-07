.PHONY: clean all

sources := $(wildcard *.c)
binaries := $(patsubst %.c,/home/$(USER)/.bin/%,$(sources))

all: $(binaries)

common_opts := -std=c99 -g -Wall -Wextra -Werror -O2
link_opts := -lm -lrt -lX11 -lpcap

clean:
	rm $(binaries)

/home/$(USER)/.bin/%: %.c
	@echo building $@
	@gcc $(common_opts) -o $@ $< $(link_opts)
