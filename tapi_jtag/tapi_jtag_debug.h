/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Debugging a core over JTAG/SWD
 *
 * @defgroup tapi_jtag_debug Core debug access (tapi_jtag_debug)
 * @ingroup tapi_jtag
 * @{
 *
 * Stopping a CPU on the chain and looking inside it: halt and resume,
 * reset, single-step, and read and write its memory and core registers.
 * This is how a test reads a boot flag out of RAM, checks a peripheral
 * register came up with its reset value, or confirms the core is running
 * the vector it should after a reset.
 *
 * The operations open one OpenOCD session each, so a test that does
 * several in a row - halt, read, write, resume - can pass them as one
 * batch to tapi_jtag_debug_session() to avoid re-attaching between them.
 *
 * @code
 * uint32_t words[4];
 * uint32_t sp;
 *
 * CHECK_RC(tapi_jtag_halt(factory, &adapter, NULL));
 * CHECK_RC(tapi_jtag_read_mem(factory, &adapter, NULL, 0x08000000, words, 4));
 * CHECK_RC(tapi_jtag_read_reg(factory, &adapter, NULL, "sp", &sp));
 * CHECK_RC(tapi_jtag_resume(factory, &adapter, NULL);
 * @endcode
 *
 * @note Halting and reset stop the device under test. A test that halts
 *       should resume or reset it again so the next test finds it
 *       running.
 */

#ifndef __TSF_TAPI_JTAG_DEBUG_H__
#define __TSF_TAPI_JTAG_DEBUG_H__

#include "tapi_jtag.h"

#ifdef __cplusplus
extern "C" {
#endif

/** How to reset a target. */
typedef enum tapi_jtag_reset_mode {
    /** Reset and run. */
    TAPI_JTAG_RESET_RUN = 0,
    /** Reset and halt at the reset vector. */
    TAPI_JTAG_RESET_HALT,
    /** Reset and halt before the first instruction (reset-init). */
    TAPI_JTAG_RESET_INIT,
} tapi_jtag_reset_mode;

/**
 * Halt the target (@c "init; halt"). @p target names the OpenOCD target
 * when there is more than one, or @c NULL for the default.
 *
 * @param factory   Job factory.
 * @param adapter   How to reach the chain.
 * @param target    Target name, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_halt(tapi_job_factory_t *factory,
                               const tapi_jtag_adapter *adapter,
                               const char *target);

/**
 * Resume the target from where it is halted.
 *
 * @param factory   Job factory.
 * @param adapter   How to reach the chain.
 * @param target    Target name, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_resume(tapi_job_factory_t *factory,
                                 const tapi_jtag_adapter *adapter,
                                 const char *target);

/**
 * Reset the target.
 *
 * @param factory   Job factory.
 * @param adapter   How to reach the chain.
 * @param mode      Reset and run, halt, or reset-init.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_reset(tapi_job_factory_t *factory,
                                const tapi_jtag_adapter *adapter,
                                tapi_jtag_reset_mode mode);

/**
 * Single-step the halted target one instruction.
 *
 * @param factory   Job factory.
 * @param adapter   How to reach the chain.
 * @param target    Target name, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_step(tapi_job_factory_t *factory,
                               const tapi_jtag_adapter *adapter,
                               const char *target);

/**
 * Read @p count 32-bit words from target memory. The target should be
 * halted; this halts it first for safety.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  adapter  How to reach the chain.
 * @param[in]  target   Target name, or @c NULL.
 * @param[in]  address  Start address.
 * @param[out] words    Buffer for @p count words.
 * @param[in]  count    Number of words to read.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_read_mem(tapi_job_factory_t *factory,
                                   const tapi_jtag_adapter *adapter,
                                   const char *target, uint64_t address,
                                   uint32_t *words, size_t count);

/**
 * Read a single 32-bit word from target memory.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  adapter  How to reach the chain.
 * @param[in]  target   Target name, or @c NULL.
 * @param[in]  address  Address.
 * @param[out] value    Where to save the word.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_read_word(tapi_job_factory_t *factory,
                                    const tapi_jtag_adapter *adapter,
                                    const char *target, uint64_t address,
                                    uint32_t *value);

/**
 * Write a single 32-bit word to target memory.
 *
 * @param factory   Job factory.
 * @param adapter   How to reach the chain.
 * @param target    Target name, or @c NULL.
 * @param address   Address.
 * @param value     Word to write.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_write_word(tapi_job_factory_t *factory,
                                     const tapi_jtag_adapter *adapter,
                                     const char *target, uint64_t address,
                                     uint32_t value);

/**
 * Read a named core register (@c "pc", @c "sp", @c "r0"...).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  adapter  How to reach the chain.
 * @param[in]  target   Target name, or @c NULL.
 * @param[in]  reg      Register name.
 * @param[out] value    Where to save the value.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_read_reg(tapi_job_factory_t *factory,
                                   const tapi_jtag_adapter *adapter,
                                   const char *target, const char *reg,
                                   uint64_t *value);

/**
 * Write a named core register.
 *
 * @param factory   Job factory.
 * @param adapter   How to reach the chain.
 * @param target    Target name, or @c NULL.
 * @param reg       Register name.
 * @param value     Value to write.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_write_reg(tapi_job_factory_t *factory,
                                    const tapi_jtag_adapter *adapter,
                                    const char *target, const char *reg,
                                    uint64_t value);

/**
 * Run a batch of debug commands in one OpenOCD session and capture the
 * output, for sequences this module does not wrap.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  adapter      How to reach the chain.
 * @param[in]  commands     OpenOCD commands, in order (@c "init" first).
 * @param[in]  n_commands   Number of @p commands.
 * @param[out] result       Result; release with tapi_jtag_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_debug_session(tapi_job_factory_t *factory,
                                        const tapi_jtag_adapter *adapter,
                                        const char **commands,
                                        size_t n_commands,
                                        tapi_jtag_result *result);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_JTAG_DEBUG_H__ */

/**@} <!-- END tapi_jtag_debug --> */
