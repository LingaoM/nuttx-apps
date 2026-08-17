############################################################################
# apps/examples/loadelf/samples/loadelf_cpp_stress/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

NUTTX_EXPORT ?= $(abspath ../../nuttx-export)
INSTALL_DIR  ?= $(abspath ../../../../bin)
SYMBOLS      ?= $(abspath ../../../../symtab_apps.c)
READELF      ?= readelf

include $(NUTTX_EXPORT)/scripts/Make.defs

PROGNAME := loadelf_cpp_stress
SRCS     := loadelf_cpp_stress.cxx
OBJS     := $(SRCS:.cxx=.o)
STACKSIZE ?= 131072

LIBCXX_READY      := $(wildcard $(NUTTX_EXPORT)/include/libcxx/algorithm)
LIBCXXABI_READY   := $(wildcard $(NUTTX_EXPORT)/include/libcxxabi/cxxabi.h)

NUTTX_INCLUDES := -isystem $(NUTTX_EXPORT)/include/libcxx
NUTTX_INCLUDES += -isystem $(NUTTX_EXPORT)/include/libcxxabi
NUTTX_INCLUDES += -isystem $(NUTTX_EXPORT)/include/cxx
NUTTX_INCLUDES += -isystem $(NUTTX_EXPORT)/include
NUTTX_DEFINES  := -D__NuttX__ -DNDEBUG
NUTTX_DEFINES  += -U_AIX -U_WIN32 -U__APPLE__ -U__FreeBSD__
NUTTX_DEFINES  += -U__NetBSD__ -U__linux__ -U__sun__ -U__unix__
NUTTX_DEFINES  += -U__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__

MODULE_CXXFLAGS  := $(ARCHOPTIMIZATION)
MODULE_CXXFLAGS  += $(filter-out -mcmodel=medium -no-pie -std=%,$(ARCHCXXFLAGS))
MODULE_CXXFLAGS  += -fvisibility=hidden -fno-pic -mcmodel=large
MODULE_CXXFLAGS  += -std=gnu++20
MODULE_CXXFLAGS  += -fno-stack-protector -fno-builtin -fno-use-cxa-atexit
MODULE_CXXFLAGS  += $(NUTTX_INCLUDES) $(NUTTX_DEFINES)

MODULE_LDFLAGS := $(filter-out -T %/gnu-elf.ld --gc-sections,$(LDELFFLAGS))
MODULE_LDFLAGS += -T $(NUTTX_EXPORT)/scripts/gnu-elf.ld
MODULE_LDFLAGS += --defsym nx_stacksize=$(STACKSIZE)

.PHONY: all clean check install check-symbols check-libcxx

all: $(PROGNAME)

check-libcxx:
	@if [ -z "$(LIBCXX_READY)" ] || [ -z "$(LIBCXXABI_READY)" ]; then \
		echo "error: NuttX libc++/libc++abi sources are not prepared"; \
		echo "       build a NuttX config with CONFIG_LIBCXX=y and CONFIG_LIBCXXABI=y first"; \
		exit 1; \
	fi

%.o: %.cxx | check-libcxx
	$(CXX) -c $(MODULE_CXXFLAGS) $< -o $@

$(PROGNAME): $(OBJS)
	$(LD) $(MODULE_LDFLAGS) $(OBJS) -o $@
	chmod 755 $@

check: $(PROGNAME)
	file $(PROGNAME)
	$(READELF) -h $(PROGNAME) | sed -n '/Type:/p;/Machine:/p'
	$(READELF) -S $(PROGNAME) | awk '$$2 == ".init_array" || \
		$$2 == ".fini_array" || $$2 == ".eh_frame" || \
		$$2 == ".gcc_except_table" { print }'
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
