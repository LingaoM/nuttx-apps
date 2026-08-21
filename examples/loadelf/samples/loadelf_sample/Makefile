############################################################################
# apps/examples/loadelf/samples/loadelf_sample/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

ifndef NUTTX_EXPORT
ifdef VELA_LOADELF_SDK
NUTTX_EXPORT := $(abspath $(VELA_LOADELF_SDK)/nuttx-export)
else
$(error NUTTX_EXPORT is not set; source the loadelf SDK env.sh first)
endif
endif

INSTALL_DIR  ?= $(abspath ../../../../bin)
SYMBOLS      ?= $(abspath ../../symbols.txt)
READELF      ?= readelf

include $(NUTTX_EXPORT)/scripts/Make.defs

PROGNAME := loadelf_sample
SRCS     := loadelf_sample.c
OBJS     := $(SRCS:.c=.o)

NUTTX_INCLUDES := -isystem $(NUTTX_EXPORT)/include
NUTTX_DEFINES  := -D__NuttX__ -DNDEBUG
NUTTX_DEFINES  += -U_AIX -U_WIN32 -U__APPLE__ -U__FreeBSD__
NUTTX_DEFINES  += -U__NetBSD__ -U__linux__ -U__sun__ -U__unix__
NUTTX_DEFINES  += -U__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__

# Build a relocatable ELF module.  The firmware loader resolves undefined
# NuttX symbols from its runtime symbol table.
MODULE_CFLAGS  := $(ARCHOPTIMIZATION)
MODULE_CFLAGS  += $(filter-out -mcmodel=medium -no-pie,$(ARCHCFLAGS))
MODULE_CFLAGS  += -fvisibility=hidden -fno-pic -mcmodel=large
MODULE_CFLAGS  += -fno-stack-protector -fno-builtin
MODULE_CFLAGS  += $(NUTTX_INCLUDES) $(NUTTX_DEFINES)

MODULE_LDFLAGS := $(filter-out -T %/gnu-elf.ld --gc-sections,$(LDELFFLAGS))
MODULE_LDFLAGS += -T $(NUTTX_EXPORT)/scripts/gnu-elf.ld

.PHONY: all clean check install check-symbols

all: $(PROGNAME)

%.o: %.c
	$(CC) -c $(MODULE_CFLAGS) $< -o $@

$(PROGNAME): $(OBJS)
	$(LD) $(MODULE_LDFLAGS) $^ -o $@
	chmod 755 $@

check: $(PROGNAME)
	file $(PROGNAME)
	$(READELF) -h $(PROGNAME) | sed -n '/Type:/p;/Machine:/p'
	$(NM) -u $(PROGNAME)
	$(MAKE) check-symbols

check-symbols: $(PROGNAME)
	@if [ -f "$(SYMBOLS)" ]; then \
		$(NM) -u $(PROGNAME) | awk '{print $$2}' | while read sym; do \
			[ -z "$$sym" ] && continue; \
			if ! grep -q "\"$$sym\"" "$(SYMBOLS)" && \
			   ! grep -qx "$$sym" "$(SYMBOLS)"; then \
				echo "warning: '$$sym' is not listed in $(SYMBOLS)"; \
			fi; \
		done; \
	else \
		echo "warning: symbols file not found: $(SYMBOLS)"; \
	fi

install: $(PROGNAME)
	mkdir -p $(INSTALL_DIR)
	cp $(PROGNAME) $(INSTALL_DIR)/$(PROGNAME)
	chmod 755 $(INSTALL_DIR)/$(PROGNAME)

clean:
	rm -f $(PROGNAME) $(OBJS)
