.PHONY: clean all

scripts_src := $(wildcard *.sh)
scripts_dst := $(patsubst %,/home/$(USER)/.sbin/%,$(scripts_src))

sources := $(wildcard *.c)
binaries := $(patsubst %.c,/home/$(USER)/.sbin/%,$(sources))

all: $(scripts_dst) $(binaries)

common_opts := -std=c99 -g -Wall -Wextra -Werror -O2
link_opts := -lm

clean:
	rm $(binaries)

/home/$(USER)/.sbin/halt: halt.c
	@echo building $@
	@gcc $(common_opts) -o $@ $< $(link_opts)
	@echo linking reboot and poweroff
	@ln -s halt /home/$(USER)/.sbin/reboot
	@ln -s halt /home/$(USER)/.sbin/poweroff

/home/$(USER)/.sbin/%: %.c
	@echo building $@
	@gcc $(common_opts) -o $@ $< $(link_opts)

/home/$(USER)/.sbin/%: %
	@echo linking $@
	@ln -s $(PWD)/$< $@
