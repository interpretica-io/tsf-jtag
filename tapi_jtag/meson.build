# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica Unipessoal Lda
#
# External TE library (see TE_EXT_REPO): engine-side TAPIs for reaching the
# JTAG/SWD debug and boundary-scan interface of a device under test from a
# Test Agent - the scan chain, core debug (halt, memory, registers), SVF
# playback and boundary scan. Built as a shared library on the engine
# platform and as a static library when agents or agent apps are built.

if get_option('engine')
    build_lib_shared = true
endif
build_lib_static = not get_option('engine') \
    or get_option('agents').split().length() != 0 \
    or get_option('agent-apps').split().length() != 0
install_lib = install_dev

headers += files(
    'tapi_jtag.h',
    'tapi_jtag_boundary.h',
    'tapi_jtag_debug.h',
    'tapi_jtag_scan.h',
    'tapi_jtag_svf.h',
)

# tapi_jtag_internal.h is internal to this library and is not installed.
sources += files(
    'tapi_jtag.c',
    'tapi_jtag_boundary.c',
    'tapi_jtag_debug.c',
    'tapi_jtag_scan.c',
    'tapi_jtag_svf.c',
)

te_libs += [
    'tapi',
    'tapi_job',
    'tapi_devtool',
]
