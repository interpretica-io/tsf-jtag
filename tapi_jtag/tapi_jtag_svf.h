/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Playing SVF and XSVF vector files
 *
 * @defgroup tapi_jtag_svf SVF / XSVF playback (tapi_jtag_svf)
 * @ingroup tapi_jtag
 * @{
 *
 * An SVF (Serial Vector Format) file is a recorded sequence of JTAG
 * shifts with the responses to check against - the portable way a CPLD is
 * programmed, an FPGA's boundary-scan test is run, or any fixed JTAG
 * procedure is replayed. XSVF is its compact binary form. OpenOCD plays
 * both and fails if a shifted-out value does not match what the file
 * expected, which is exactly the pass/fail a test wants.
 *
 * @code
 * bool ok;
 *
 * CHECK_RC(tapi_jtag_svf_play(factory, &adapter, "/tmp/cpld.svf", &ok));
 * if (!ok)
 *     TEST_VERDICT("The SVF did not verify against the device");
 * @endcode
 *
 * @note The SVF or XSVF file is a path on the agent the probe is wired
 *       to; put it there first with TE's @c tapi_file_copy_ta().
 */

#ifndef __TSF_TAPI_JTAG_SVF_H__
#define __TSF_TAPI_JTAG_SVF_H__

#include "tapi_jtag.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Play an SVF file onto the chain, checking every expected response.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  adapter  How to reach the chain.
 * @param[in]  path     Path of the SVF file on the agent.
 * @param[out] ok       Where to save whether it played and verified.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_svf_play(tapi_job_factory_t *factory,
                                   const tapi_jtag_adapter *adapter,
                                   const char *path, bool *ok);

/**
 * Play an XSVF (binary) file onto the chain.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  adapter  How to reach the chain.
 * @param[in]  path     Path of the XSVF file on the agent.
 * @param[out] ok       Where to save whether it played and verified.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_xsvf_play(tapi_job_factory_t *factory,
                                    const tapi_jtag_adapter *adapter,
                                    const char *path, bool *ok);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_JTAG_SVF_H__ */

/**@} <!-- END tapi_jtag_svf --> */
