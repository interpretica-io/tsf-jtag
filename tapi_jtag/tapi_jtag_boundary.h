/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Boundary scan
 *
 * @defgroup tapi_jtag_boundary Boundary scan (tapi_jtag_boundary)
 * @ingroup tapi_jtag
 * @{
 *
 * Boundary scan reaches the pins of a part through its JTAG TAP, with no
 * firmware running: SAMPLE reads what every pin sees, EXTEST drives them.
 * It is how a board is tested before it can boot - that a trace is not
 * open or shorted, that a device is soldered down, that a pin toggles -
 * and it is driven here through @c jtag (UrJTAG), which reads the part's
 * BSDL to know the boundary register.
 *
 * @code
 * tapi_jtag_boundary_opt opt = tapi_jtag_boundary_default_opt;
 * te_string signals = TE_STRING_INIT;
 *
 * opt.cable = "jlink";
 * opt.part_bsdl = "/opt/bsdl/stm32f407.bsdl";
 * CHECK_RC(tapi_jtag_boundary_sample(factory, &opt, &signals));
 * @endcode
 *
 * @note UrJTAG is a different tool from OpenOCD, with its own cable
 *       names; the @ref tapi_jtag_adapter of the rest of this library is
 *       not used here. A BSDL file for the part is required.
 */

#ifndef __TSF_TAPI_JTAG_BOUNDARY_H__
#define __TSF_TAPI_JTAG_BOUNDARY_H__

#include "tapi_jtag.h"

#ifdef __cplusplus
extern "C" {
#endif

/** How to reach a part for boundary scan through UrJTAG. */
typedef struct tapi_jtag_boundary_opt {
    /** UrJTAG cable name, e.g. @c "jlink", @c "ft2232". */
    const char *cable;
    /** Extra @c cable parameters, e.g. @c "vid=0x0403 pid=0x6010", or NULL. */
    const char *cable_params;
    /** Path to the part's BSDL file on the agent. */
    const char *part_bsdl;
    /** Position of the part on the chain (0 nearest TDO). */
    unsigned int part_index;
} tapi_jtag_boundary_opt;

/** Defaults: part at index 0, no extra cable parameters. */
extern const tapi_jtag_boundary_opt tapi_jtag_boundary_default_opt;

/**
 * SAMPLE the boundary register: read the state of every boundary-scan
 * pin, without disturbing it.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  opt      How to reach the part.
 * @param[out] signals  String to append the tool's per-signal output to.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_boundary_sample(tapi_job_factory_t *factory,
                                          const tapi_jtag_boundary_opt *opt,
                                          te_string *signals);

/**
 * Read one boundary-scan signal by its BSDL name.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  opt      How to reach the part.
 * @param[in]  signal   Signal (pin) name from the BSDL.
 * @param[out] value    Where to save the bit read.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_boundary_get_signal(
                                    tapi_job_factory_t *factory,
                                    const tapi_jtag_boundary_opt *opt,
                                    const char *signal, bool *value);

/**
 * Drive one boundary-scan signal (EXTEST) and read it back to confirm.
 *
 * @param factory   Job factory.
 * @param opt       How to reach the part.
 * @param signal    Signal (pin) name from the BSDL.
 * @param value     Bit to drive.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_boundary_set_signal(
                                    tapi_job_factory_t *factory,
                                    const tapi_jtag_boundary_opt *opt,
                                    const char *signal, bool value);

/**
 * Run a batch of UrJTAG commands (its own command language) after the
 * cable is opened and the part is detected, and capture the output.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          How to reach the part.
 * @param[in]  commands     UrJTAG commands, one per line.
 * @param[out] result       Result; release with tapi_jtag_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_boundary_run(tapi_job_factory_t *factory,
                                       const tapi_jtag_boundary_opt *opt,
                                       const char *commands,
                                       tapi_jtag_result *result);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_JTAG_BOUNDARY_H__ */

/**@} <!-- END tapi_jtag_boundary --> */
