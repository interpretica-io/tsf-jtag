/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief The JTAG scan chain
 *
 * @defgroup tapi_jtag_scan Scan chain (tapi_jtag_scan)
 * @ingroup tapi_jtag
 * @{
 *
 * What is actually on the chain: the TAPs, in order, with the IDCODE each
 * one reported. This is the first thing a board bring-up test checks -
 * that every part is present, in the right place, and reads the ID it
 * should. A missing TAP, a wrong IDCODE or an all-ones IDCODE (a part
 * held in reset, or a broken TDI/TDO trace) is caught here before
 * anything else is tried.
 *
 * @code
 * tapi_jtag_chain chain;
 * static const uint32_t expected[] = { 0x06413041, 0x4ba00477 };
 *
 * CHECK_RC(tapi_jtag_scan(factory, &adapter, &chain));
 * tapi_jtag_chain_log(&chain);
 * if (!tapi_jtag_chain_matches(&chain, expected, TE_ARRAY_LEN(expected)))
 *     TEST_VERDICT("The scan chain is not the board that was expected");
 * tapi_jtag_chain_free(&chain);
 * @endcode
 */

#ifndef __TSF_TAPI_JTAG_SCAN_H__
#define __TSF_TAPI_JTAG_SCAN_H__

#include "tapi_jtag.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One TAP (test access port) on the chain. */
typedef struct tapi_jtag_tap {
    /** Position on the chain, 0 nearest TDO. */
    unsigned int index;
    /** Dotted name OpenOCD gives it, e.g. @c "stm32f4x.cpu". */
    char *name;
    /** IDCODE read from the part. */
    uint32_t idcode;
    /** IDCODE the configuration expected (0 if it declared none). */
    uint32_t expected;
    /** Instruction register length, bits. */
    unsigned int irlen;
    /** @c true if the TAP is enabled. */
    bool enabled;
} tapi_jtag_tap;

/** The scan chain. */
typedef struct tapi_jtag_chain {
    /** Vector of #tapi_jtag_tap, in chain order. */
    te_vec taps;
} tapi_jtag_chain;

/**
 * Read the scan chain: initialise the adapter and enumerate the TAPs.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  adapter  How to reach the chain.
 * @param[out] chain    Chain; release with tapi_jtag_chain_free().
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_scan(tapi_job_factory_t *factory,
                               const tapi_jtag_adapter *adapter,
                               tapi_jtag_chain *chain);

/**
 * Parse the output of OpenOCD's @c scan_chain command into a chain.
 *
 * @param[in]  text     Output of @c scan_chain.
 * @param[out] chain    Chain to fill; init with tapi_jtag_chain_init().
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_scan_parse(const char *text,
                                     tapi_jtag_chain *chain);

/**
 * Prepare an empty chain.
 *
 * @param chain     Chain.
 */
extern void tapi_jtag_chain_init(tapi_jtag_chain *chain);

/**
 * Find a TAP by (a suffix of) its name, e.g. @c "cpu".
 *
 * @param chain     Chain.
 * @param name      Name or a dotted suffix of it.
 *
 * @return The TAP, or @c NULL.
 */
extern const tapi_jtag_tap *tapi_jtag_chain_find(const tapi_jtag_chain *chain,
                                                 const char *name);

/**
 * Whether the chain's IDCODEs are exactly @p expected, in order.
 *
 * @param chain     Chain.
 * @param expected  Expected IDCODEs, in chain order.
 * @param n         Number of @p expected.
 *
 * @return @c true if the count and every IDCODE match.
 */
extern bool tapi_jtag_chain_matches(const tapi_jtag_chain *chain,
                                    const uint32_t *expected, size_t n);

/**
 * Whether any TAP read an unusable IDCODE - all zeros or all ones - which
 * means a part held in reset, unpowered, or a broken TDI/TDO trace.
 *
 * @param[in]  chain    Chain.
 * @param[out] which    Name of the first bad TAP (may be @c NULL).
 *
 * @return @c true if a bad IDCODE was found.
 */
extern bool tapi_jtag_chain_has_bad_idcode(const tapi_jtag_chain *chain,
                                           const char **which);

/**
 * Write the chain into the log, one line per TAP.
 *
 * @param chain     Chain.
 */
extern void tapi_jtag_chain_log(const tapi_jtag_chain *chain);

/**
 * Release a chain.
 *
 * @param chain     Chain.
 */
extern void tapi_jtag_chain_free(tapi_jtag_chain *chain);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_JTAG_SCAN_H__ */

/**@} <!-- END tapi_jtag_scan --> */
