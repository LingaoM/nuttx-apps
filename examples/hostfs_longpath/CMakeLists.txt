# ##############################################################################
# apps/examples/hostfs_longpath/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_EXAMPLES_HOSTFS_LONGPATH)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_HOSTFS_LONGPATH_PROGNAME}
    SRCS
    hostfs_longpath_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_HOSTFS_LONGPATH_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_HOSTFS_LONGPATH_PRIORITY})
endif()
