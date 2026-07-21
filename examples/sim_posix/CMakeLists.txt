############################################################################
# apps/examples/sim_posix/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
############################################################################

if(CONFIG_EXAMPLES_SIM_POSIX)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_SIM_POSIX_PROGNAME}
    SRCS
    sim_posix_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_SIM_POSIX_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_SIM_POSIX_PRIORITY})
endif()
