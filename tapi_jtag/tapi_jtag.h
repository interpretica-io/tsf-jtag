/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief JTAG and SWD debug access from a Test Agent
 *
 * @defgroup tapi_jtag JTAG / SWD debug access (tapi_jtag)
 * @{
 *
 * Reaching the debug and boundary-scan interface of a device under test
 * from a Test Agent, over a debug probe wired to it. This is the bench
 * way into a board that has no network and no console yet: read the scan
 * chain to prove the parts are there and wired, halt a core and read its
 * memory and registers, play an SVF onto a CPLD, or drive the pins
 * directly with a boundary scan.
 *
 * The pieces:
 *
 * - this group: how a probe and the chain behind it are described, which
 *   tools the agent has, and running a batch of commands;
 * - @ref tapi_jtag_scan - the scan chain: the TAPs on it and their
 *   IDCODEs, and whether they are the ones expected;
 * - @ref tapi_jtag_debug - a CPU on the chain: halt, resume, reset,
 *   step, and read and write its memory and registers;
 * - @ref tapi_jtag_svf - playing an SVF or XSVF vector file onto the
 *   chain, the usual way a CPLD or a board test is driven;
 * - @ref tapi_jtag_boundary - boundary scan: sampling the pins of a part
 *   and driving them, for board bring-up without any firmware.
 *
 * Almost everything goes through @c openocd, the tool every probe speaks;
 * the boundary scan can also go through @c jtag (UrJTAG). The tool runs
 * on the agent the probe is attached to, which may be the engine host.
 *
 * @code
 * tapi_jtag_adapter adapter = tapi_jtag_adapter_default;
 * static const char *cfg[] = { "interface/stlink.cfg",
 *                              "target/stm32f4x.cfg" };
 * tapi_jtag_chain chain;
 *
 * adapter.configs = cfg;
 * adapter.n_configs = 2;
 * CHECK_RC(tapi_jtag_scan(factory, &adapter, &chain));
 * ... assert on chain.taps ...
 * tapi_jtag_chain_free(&chain);
 * @endcode
 *
 * @note The probe drives a device electrically and can hold it in reset
 *       or halt it. Everything here points at the board the suite's own
 *       configuration names, over the probe the agent owns; that is the
 *       engagement it belongs to.
 */

#ifndef __TSF_TAPI_JTAG_H__
#define __TSF_TAPI_JTAG_H__

#include <stdint.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "te_vector.h"
#include "tapi_job.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default timeout of a JTAG operation, ms. */
#define TAPI_JTAG_TIMEOUT_MS        120000

/** The transport a probe uses to reach the chip. */
typedef enum tapi_jtag_transport {
    /** Let the configuration decide (the usual case). */
    TAPI_JTAG_TRANSPORT_DEFAULT = 0,
    /** JTAG (the full boundary-scan chain). */
    TAPI_JTAG_TRANSPORT_JTAG,
    /** SWD (ARM's two-wire debug). */
    TAPI_JTAG_TRANSPORT_SWD,
} tapi_jtag_transport;

/** How to reach a scan chain: the probe, the target and the transport. */
typedef struct tapi_jtag_adapter {
    /** OpenOCD config files (@c -f): the interface, then the target. */
    const char **configs;
    /** Number of @a configs. */
    size_t n_configs;
    /** Commands to run before the configs (@c -c), or @c NULL. */
    const char **pre_commands;
    /** Number of @a pre_commands. */
    size_t n_pre_commands;
    /** Search path for configs (@c -s), or @c NULL. */
    const char *search_dir;
    /** Transport to select, or DEFAULT. */
    tapi_jtag_transport transport;
    /** Adapter clock, kHz; 0 to leave it to the configuration. */
    unsigned int speed_khz;
} tapi_jtag_adapter;

/** Defaults: no extra commands, the configuration's transport and speed. */
extern const tapi_jtag_adapter tapi_jtag_adapter_default;

/** Which JTAG tools an agent has. */
typedef struct tapi_jtag_tools {
    bool openocd;       /**< @c openocd */
    bool urjtag;        /**< @c jtag (UrJTAG) */
    bool openfpgaloader;/**< @c openFPGALoader */
} tapi_jtag_tools;

/**
 * Find out which JTAG tools the agent has.
 *
 * @param[in]  factory  Job factory.
 * @param[out] tools    Where to save the answer.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_tools_probe(tapi_job_factory_t *factory,
                                      tapi_jtag_tools *tools);

/** What one JTAG operation did. */
typedef struct tapi_jtag_result {
    /** @c true if the tool succeeded and printed no error. */
    bool ok;
    /** Exit status of the tool, or @c -1 if it did not exit normally. */
    int status;
    /** Everything the tool printed, kept for the log and for parsing. */
    te_string output;
} tapi_jtag_result;

/**
 * Prepare an empty result.
 *
 * @param result    Result.
 */
extern void tapi_jtag_result_init(tapi_jtag_result *result);

/**
 * Release a result.
 *
 * @param result    Result.
 */
extern void tapi_jtag_result_free(tapi_jtag_result *result);

/**
 * Run a batch of OpenOCD commands over the adapter and capture the
 * output. The batch always ends with @c shutdown, so OpenOCD does not
 * wait for a connection.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  adapter      How to reach the chain.
 * @param[in]  commands     Commands to run in order.
 * @param[in]  n_commands   Number of @p commands.
 * @param[out] result       Result; release with tapi_jtag_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_run(tapi_job_factory_t *factory,
                              const tapi_jtag_adapter *adapter,
                              const char **commands, size_t n_commands,
                              tapi_jtag_result *result);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_JTAG_H__ */

/**@} <!-- END tapi_jtag --> */
