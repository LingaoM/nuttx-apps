############################################################################
# apps/examples/loadelf/samples/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

NUTTX_EXPORT ?= $(abspath ../nuttx-export)
INSTALL_DIR  ?= $(abspath ../../../bin)
SYMBOLS      ?= $(abspath ../symbols.txt)

include $(NUTTX_EXPORT)/scripts/Make.defs

SAMPLE_DIRS := $(sort $(patsubst %/Makefile,%,$(wildcard */Makefile)))
SAMPLE_BINS := $(addprefix $(INSTALL_DIR)/,$(SAMPLE_DIRS))

.PHONY: all clean check install list symbols
.PHONY: $(addprefix clean-,$(SAMPLE_DIRS))
.PHONY: $(addprefix check-,$(SAMPLE_DIRS))
.PHONY: $(addprefix install-,$(SAMPLE_DIRS))

all: install symbols

install: $(addprefix install-,$(SAMPLE_DIRS))

symbols: install
	@tmp="$(SYMBOLS).tmp"; \
	export LC_ALL=C; \
	$(NM) -u $(SAMPLE_BINS) 2>/dev/null | \
		awk '$$1 == "U" || $$1 == "w" || $$1 == "v" { print $$2 }' | \
		sort -u > "$$tmp"; \
	mv "$$tmp" "$(SYMBOLS)"; \
	echo "Generated $(SYMBOLS) from $(words $(SAMPLE_BINS)) samples"

check: symbols $(addprefix check-,$(SAMPLE_DIRS))

clean: $(addprefix clean-,$(SAMPLE_DIRS))

list:
	@printf '%s\n' $(SAMPLE_DIRS)

$(addprefix install-,$(SAMPLE_DIRS)): install-%:
	$(MAKE) -C $* NUTTX_EXPORT="$(NUTTX_EXPORT)" \
		INSTALL_DIR="$(INSTALL_DIR)" install

$(addprefix check-,$(SAMPLE_DIRS)): check-%:
	$(MAKE) -C $* NUTTX_EXPORT="$(NUTTX_EXPORT)" \
		INSTALL_DIR="$(INSTALL_DIR)" SYMBOLS="$(SYMBOLS)" check

$(addprefix clean-,$(SAMPLE_DIRS)): clean-%:
	$(MAKE) -C $* NUTTX_EXPORT="$(NUTTX_EXPORT)" \
		INSTALL_DIR="$(INSTALL_DIR)" clean
