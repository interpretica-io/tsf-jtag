/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Debugging a core over JTAG/SWD
 *
 * Implementation of halt/resume/reset/step and memory and register
 * access, built on OpenOCD command batches.
 */

#define TE_LGR_USER "TAPI JTAG DEBUG"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_jtag_debug.h"
#include "tapi_jtag_internal.h"

/** Run a command batch, optionally selecting a target after init. */
static te_errno
debug_run(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
          const char *target, const char **extra, size_t n_extra,
          tapi_jtag_result *result)
{
    te_vec cmds = TE_VEC_INIT(const char *);
    te_string sel = TE_STRING_INIT;
    const char *init = "init";
    size_t i;
    te_errno rc;

    TE_VEC_APPEND(&cmds, init);
    if (target != NULL)
    {
        const char *s;

        te_string_append(&sel, "targets %s", target);
        s = sel.ptr;
        TE_VEC_APPEND(&cmds, s);
    }
    for (i = 0; i < n_extra; i++)
        TE_VEC_APPEND(&cmds, extra[i]);

    rc = tapi_jtag_run(factory, adapter,
                       (const char **)te_vec_get_mutable(&cmds, 0),
                       te_vec_size(&cmds), result);
    te_vec_free(&cmds);
    te_string_free(&sel);

    return rc;
}

/** Run a batch that must succeed, discarding the output. */
static te_errno
debug_do(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
         const char *target, const char **extra, size_t n_extra)
{
    tapi_jtag_result result;
    te_errno rc;

    rc = debug_run(factory, adapter, target, extra, n_extra, &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    tapi_jtag_result_free(&result);

    return rc;
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_halt(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
               const char *target)
{
    const char *extra[] = { "halt" };

    return debug_do(factory, adapter, target, extra, TE_ARRAY_LEN(extra));
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_resume(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
                 const char *target)
{
    const char *extra[] = { "resume" };

    return debug_do(factory, adapter, target, extra, TE_ARRAY_LEN(extra));
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_reset(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
                tapi_jtag_reset_mode mode)
{
    const char *extra[1];

    extra[0] = (mode == TAPI_JTAG_RESET_HALT) ? "reset halt" :
               (mode == TAPI_JTAG_RESET_INIT) ? "reset init" : "reset run";

    return debug_do(factory, adapter, NULL, extra, 1);
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_step(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
               const char *target)
{
    const char *extra[] = { "step" };

    return debug_do(factory, adapter, target, extra, TE_ARRAY_LEN(extra));
}

/** Parse "0xADDR: w0 w1 w2 w3" lines from mdw output into @p words. */
static size_t
parse_mdw(const char *text, uint32_t *words, size_t count)
{
    const char *pos = text;
    const char *line;
    size_t len;
    size_t got = 0;

    while ((line = tapi_jtag_next_line(&pos, &len)) != NULL && got < count)
    {
        char buf[512];
        const char *colon;
        char *save = NULL;
        char *t;

        if (len == 0 || len >= sizeof(buf))
            continue;
        memcpy(buf, line, len);
        buf[len] = '\0';

        colon = strchr(buf, ':');
        /* Only lines that begin with an address and a colon. */
        if (colon == NULL || strncmp(buf, "0x", 2) != 0)
            continue;

        for (t = strtok_r((char *)colon + 1, " \t", &save);
             t != NULL && got < count; t = strtok_r(NULL, " \t", &save))
        {
            if (strncmp(t, "0x", 2) == 0)
                words[got++] = (uint32_t)strtoul(t, NULL, 16);
            else if ((t[0] >= '0' && t[0] <= '9') ||
                     (t[0] >= 'a' && t[0] <= 'f') ||
                     (t[0] >= 'A' && t[0] <= 'F'))
                words[got++] = (uint32_t)strtoul(t, NULL, 16);
        }
    }

    return got;
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_read_mem(tapi_job_factory_t *factory,
                   const tapi_jtag_adapter *adapter, const char *target,
                   uint64_t address, uint32_t *words, size_t count)
{
    tapi_jtag_result result;
    const char *extra[2];
    char *mdw;
    size_t got;
    te_errno rc;

    extra[0] = "halt";
    mdw = te_string_fmt("mdw 0x%" PRIx64 " %zu", address, count);
    extra[1] = mdw;

    rc = debug_run(factory, adapter, target, extra, 2, &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    if (rc == 0)
    {
        got = parse_mdw(result.output.ptr != NULL ? result.output.ptr : "",
                        words, count);
        if (got != count)
        {
            ERROR("Read %zu of %zu words from 0x%" PRIx64, got, count,
                  address);
            rc = TE_RC(TE_TAPI, TE_EFAIL);
        }
    }

    free(mdw);
    tapi_jtag_result_free(&result);

    return rc;
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_read_word(tapi_job_factory_t *factory,
                    const tapi_jtag_adapter *adapter, const char *target,
                    uint64_t address, uint32_t *value)
{
    return tapi_jtag_read_mem(factory, adapter, target, address, value, 1);
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_write_word(tapi_job_factory_t *factory,
                     const tapi_jtag_adapter *adapter, const char *target,
                     uint64_t address, uint32_t value)
{
    const char *extra[2];
    char *mww;
    te_errno rc;

    extra[0] = "halt";
    mww = te_string_fmt("mww 0x%" PRIx64 " 0x%" PRIx32, address, value);
    extra[1] = mww;

    rc = debug_do(factory, adapter, target, extra, 2);
    free(mww);

    return rc;
}

/** Parse "name (/bits): 0xVALUE" from reg output. */
static bool
parse_reg(const char *text, uint64_t *value)
{
    const char *pos = text;
    const char *line;
    size_t len;

    while ((line = tapi_jtag_next_line(&pos, &len)) != NULL)
    {
        const char *hex = NULL;
        const char *p;

        /* The last "0x..." token on a line naming the register. */
        for (p = line; p + 2 < line + len; p++)
        {
            if (p[0] == '0' && p[1] == 'x')
                hex = p;
        }
        if (hex != NULL)
        {
            *value = strtoull(hex, NULL, 16);
            return true;
        }
    }

    return false;
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_read_reg(tapi_job_factory_t *factory,
                   const tapi_jtag_adapter *adapter, const char *target,
                   const char *reg, uint64_t *value)
{
    tapi_jtag_result result;
    const char *extra[2];
    char *cmd;
    te_errno rc;

    extra[0] = "halt";
    cmd = te_string_fmt("reg %s", reg);
    extra[1] = cmd;

    rc = debug_run(factory, adapter, target, extra, 2, &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    if (rc == 0 &&
        !parse_reg(result.output.ptr != NULL ? result.output.ptr : "", value))
    {
        ERROR("Cannot read register %s: %s", reg,
              result.output.ptr != NULL ? result.output.ptr : "");
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    }

    free(cmd);
    tapi_jtag_result_free(&result);

    return rc;
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_write_reg(tapi_job_factory_t *factory,
                    const tapi_jtag_adapter *adapter, const char *target,
                    const char *reg, uint64_t value)
{
    const char *extra[2];
    char *cmd;
    te_errno rc;

    extra[0] = "halt";
    cmd = te_string_fmt("reg %s 0x%" PRIx64, reg, value);
    extra[1] = cmd;

    rc = debug_do(factory, adapter, target, extra, 2);
    free(cmd);

    return rc;
}

/* See description in tapi_jtag_debug.h */
te_errno
tapi_jtag_debug_session(tapi_job_factory_t *factory,
                        const tapi_jtag_adapter *adapter,
                        const char **commands, size_t n_commands,
                        tapi_jtag_result *result)
{
    return tapi_jtag_run(factory, adapter, commands, n_commands, result);
}
