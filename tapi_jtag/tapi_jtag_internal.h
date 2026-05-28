/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief JTAG TAPI: internal helpers
 *
 * Internal to tsf-jtag; not installed.
 *
 * Every operation ends up running OpenOCD (or another JTAG tool) on the
 * agent the debug probe is wired to, and reading what it printed.
 * tsf-devtool already knows how to run a tool and capture its output;
 * this is the shape on top of it.
 */

#ifndef __TSF_TAPI_JTAG_INTERNAL_H__
#define __TSF_TAPI_JTAG_INTERNAL_H__

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "te_vector.h"
#include "tapi_job.h"

#include "tapi_jtag.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run @p program with @p args on the agent, wait for it and capture both
 * output streams.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  name         Tool name for log messages.
 * @param[in]  program      Program name or path.
 * @param[in]  args         Arguments after @c argv[0] (may be @c NULL).
 * @param[in]  n_args       Number of @p args.
 * @param[in]  timeout_ms   Timeout, ms.
 * @param[out] out          String to append stdout to (may be @c NULL).
 * @param[out] err          String to append stderr to (may be @c NULL).
 * @param[out] status       Exit status, or @c -1 (may be @c NULL).
 *
 * @return Status code of running the program, not of the program.
 */
extern te_errno tapi_jtag_run_tool(tapi_job_factory_t *factory,
                                   const char *name, const char *program,
                                   const char **args, size_t n_args,
                                   int timeout_ms, te_string *out,
                                   te_string *err, int *status);

/**
 * Run a shell script: @c sh @c -c @p script @c sh @p args. Values reach
 * the script as positional parameters, never pasted into its text.
 *
 * Parameters are those of tapi_jtag_run_tool().
 *
 * @return Status code of running the script, not of the script.
 */
extern te_errno tapi_jtag_sh(tapi_job_factory_t *factory, const char *name,
                             const char *script, const char **args,
                             size_t n_args, int timeout_ms, te_string *out,
                             te_string *err, int *status);

/**
 * Check whether a program is on the agent, i.e. @c command @c -v.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  program      Program name.
 * @param[out] present      Where to save the answer.
 *
 * @return Status code.
 */
extern te_errno tapi_jtag_have_tool(tapi_job_factory_t *factory,
                                    const char *program, bool *present);

/**
 * Split off the next line of a buffer.
 *
 * @param[in,out] pos       Position; advanced past the line, @c NULL at
 *                          the end.
 * @param[out]    len       Length of the line without the newline.
 *
 * @return Start of the line, or @c NULL when there is none left.
 */
extern const char *tapi_jtag_next_line(const char **pos, size_t *len);

/**
 * Build the OpenOCD argument vector from an adapter and a list of
 * commands, ending with @c shutdown, into @p args and its backing
 * @p pool.
 *
 * @param[in]  adapter      How to reach the chain.
 * @param[in]  commands     Commands to run in order.
 * @param[in]  n_commands   Number of @p commands.
 * @param[out] args         Vector of @c const @c char @c * to append to.
 * @param[out] pool         Vector of @c char @c * owning formatted args.
 */
extern void tapi_jtag_ocd_args(const tapi_jtag_adapter *adapter,
                               const char **commands, size_t n_commands,
                               te_vec *args, te_vec *pool);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_JTAG_INTERNAL_H__ */
