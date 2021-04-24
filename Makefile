__nmk_dir=$(CURDIR)/scripts/nmk/scripts/
export __nmk_dir

#
# No need to try to remake our Makefiles
Makefile: ;
Makefile.%: ;
scripts/%.mak: ;
$(__nmk_dir)%.mk: ;

#
# Import the build engine
include $(__nmk_dir)include.mk
include $(__nmk_dir)macro.mk

DEFINES			+= -D_FILE_OFFSET_BITS=64
DEFINES			+= -D_GNU_SOURCE

WARNINGS		:= -Wall -Wformat-security -Wdeclaration-after-statement -Wstrict-prototypes

ifneq ($(WERROR),0)
        WARNINGS	+= -Werror
endif

ifeq ($(DEBUG),1)
        DEFINES		+= -DDEBUG
        CFLAGS		+= -O0 -ggdb3
else
        CFLAGS		+= -O2 -g
endif

CFLAGS			+= $(WARNINGS) $(DEFINES) -std=c99 -iquote include/
export CFLAGS

src/dcf77gen/Makefile: ;
src/dcf77gen/%: .FORCE
	$(Q) $(MAKE) $(build)=src/dcf77gen $@

dcf77gen-libs-y		+= -lm
dcf77gen: src/dcf77gen/built-in.o
	$(call msg-link, $@)
	$(Q) $(CC) $(CFLAGS) $^ $(WRAPFLAGS) $(LDFLAGS) $(dcf77gen-libs-y) -rdynamic -o $@

src/dcf77sync/Makefile: ;
src/dcf77sync/%: .FORCE
	$(Q) $(MAKE) $(build)=src/dcf77sync $@

play-gen-c += src/dcf77sync/play_0105.c
play-gen-c += src/dcf77sync/play_0205.c
play-gen-c += src/dcf77sync/play_0810.c
play-gen-c += src/dcf77sync/play_0910.c
play-gen-c += src/dcf77sync/play_1010.c

src/dcf77sync/play_0105.c: dcf77gen
	$(call msg-gen, $@)
	$(Q) ./dcf77gen $@

src/dcf77sync/play_0205.c: dcf77gen
	$(call msg-gen, $@)
	$(Q) ./dcf77gen $@

src/dcf77sync/play_0810.c: dcf77gen
	$(call msg-gen, $@)
	$(Q) ./dcf77gen $@

src/dcf77sync/play_0910.c: dcf77gen
	$(call msg-gen, $@)
	$(Q) ./dcf77gen $@

src/dcf77sync/play_1010.c: dcf77gen
	$(call msg-gen, $@)
	$(Q) ./dcf77gen $@

src/dcf77sync/built-in.o: | $(play-gen-c)
src/dcf77sync/%.d: | $(play-gen-c)

dcf77sync-libs-y		+= -lrt -lpulse-simple
dcf77sync: src/dcf77sync/built-in.o
	$(call msg-link, $@)
	$(Q) $(CC) $(CFLAGS) $^ $(WRAPFLAGS) $(LDFLAGS) $(dcf77sync-libs-y) -rdynamic -o $@

clean:
	$(Q) $(RM) dcf77sync
	$(Q) $(RM) dcf77gen
	$(Q) $(RM) $(play-gen-c)
	$(Q) $(MAKE) $(build)=src/dcf77gen $@
	$(Q) $(MAKE) $(build)=src/dcf77sync $@
.PHONY: clean

all: dcf77sync

.DEFAULT_GOAL := all

# Disable implicit rules in _this_ Makefile.
.SUFFIXES:
