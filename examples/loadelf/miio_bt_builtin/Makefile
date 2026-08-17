############################################################################
# apps/examples/loadelf/miio_bt_builtin/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = miio_bt
PRIORITY  = $(CONFIG_EXAMPLES_LOADELF_MIIO_BT_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_LOADELF_MIIO_BT_STACKSIZE)
APPMAIN   = miio_bt_main

MIIO_BT_LIB = $(patsubst "%",%,$(CONFIG_EXAMPLES_LOADELF_MIIO_BT_LIB))

all::
	@:

install::
	@:

postinstall::
	@:

context:: $(MIIO_BT_LIB)
	@:

register::
	$(call REGISTER,$(PROGNAME),$(PRIORITY),$(STACKSIZE),$(APPMAIN))

depend::
	@:

clean::
	@:

distclean:: clean
	@:
