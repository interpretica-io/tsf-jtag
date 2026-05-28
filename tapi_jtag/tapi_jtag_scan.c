/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief The JTAG scan chain
 *
 * Implementation of the scan-chain enumeration and its checks.
 */

#define TE_LGR_USER "TAPI JTAG SCAN"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_jtag_scan.h"
#include "tapi_jtag_internal.h"

/** Release one TAP. */
static void
tap_destroy(const void *item)
{
    free(((const tapi_jtag_tap *)item)->name);
}

/* See description in tapi_jtag_scan.h */
void
tapi_jtag_chain_init(tapi_jtag_chain *chain)
{
    chain->taps = (te_vec)TE_VEC_INIT_DESTROY(tapi_jtag_tap, tap_destroy);
}

/* See description in tapi_jtag_scan.h */
void
tapi_jtag_chain_free(tapi_jtag_chain *chain)
{
    te_vec_free(&chain->taps);
}

/* See description in tapi_jtag_scan.h */
te_errno
tapi_jtag_scan_parse(const char *text, tapi_jtag_chain *chain)
{
    const char *pos = text;
    const char *line;
    size_t len;

    /*
     *    TapName            Enabled IdCode     Expected   IrLen IrCap IrMask
     * -- ------------------ ------- ---------- ---------- ----- ----- ------
     *  0 stm32f4x.bs           Y    0x06413041 0x06413041     5 0x01  0x03
     *  1 stm32f4x.cpu          Y    0x4ba00477 0x4ba00477     4 0x01  0x0f
     */
    while ((line = tapi_jtag_next_line(&pos, &len)) != NULL)
    {
        char buf[512];
        char *tok[12];
        size_t n = 0;
        char *save = NULL;
        char *t;
        tapi_jtag_tap tap;

        if (len == 0 || len >= sizeof(buf))
            continue;
        memcpy(buf, line, len);
        buf[len] = '\0';

        for (t = strtok_r(buf, " \t", &save); t != NULL && n < TE_ARRAY_LEN(tok);
             t = strtok_r(NULL, " \t", &save))
            tok[n++] = t;

        /* A data row: index, name, Y/N, 0x..idcode, 0x..expected, irlen. */
        if (n < 6)
            continue;
        if (tok[0][0] < '0' || tok[0][0] > '9')
            continue;
        if (strncmp(tok[3], "0x", 2) != 0)
            continue;

        memset(&tap, 0, sizeof(tap));
        tap.index = (unsigned int)strtoul(tok[0], NULL, 10);
        tap.name = TE_STRDUP(tok[1]);
        tap.enabled = (tok[2][0] == 'Y' || tok[2][0] == 'y');
        tap.idcode = (uint32_t)strtoul(tok[3], NULL, 16);
        tap.expected = (strncmp(tok[4], "0x", 2) == 0) ?
                       (uint32_t)strtoul(tok[4], NULL, 16) : 0;
        tap.irlen = (unsigned int)strtoul(tok[5], NULL, 10);
        TE_VEC_APPEND(&chain->taps, tap);
    }

    return 0;
}

/* See description in tapi_jtag_scan.h */
te_errno
tapi_jtag_scan(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
               tapi_jtag_chain *chain)
{
    const char *commands[] = { "init", "scan_chain" };
    tapi_jtag_result result;
    te_errno rc;

    tapi_jtag_chain_init(chain);

    rc = tapi_jtag_run(factory, adapter, commands, TE_ARRAY_LEN(commands),
                       &result);
    if (rc == 0)
    {
        if (result.output.ptr != NULL)
            rc = tapi_jtag_scan_parse(result.output.ptr, chain);
        if (te_vec_size(&chain->taps) == 0)
        {
            ERROR("No TAP found on the scan chain: %s",
                  result.output.ptr != NULL ? result.output.ptr : "");
            rc = TE_RC(TE_TAPI, TE_ENOENT);
        }
    }
    tapi_jtag_result_free(&result);
    if (rc != 0)
        tapi_jtag_chain_free(chain);

    return rc;
}

/* See description in tapi_jtag_scan.h */
const tapi_jtag_tap *
tapi_jtag_chain_find(const tapi_jtag_chain *chain, const char *name)
{
    const tapi_jtag_tap *tap;
    size_t nlen = strlen(name);

    TE_VEC_FOREACH(&chain->taps, tap)
    {
        size_t tlen = strlen(tap->name);

        if (strcmp(tap->name, name) == 0)
            return tap;
        /* Match a dotted suffix: "cpu" against "stm32f4x.cpu". */
        if (tlen > nlen && tap->name[tlen - nlen - 1] == '.' &&
            strcmp(tap->name + tlen - nlen, name) == 0)
            return tap;
    }

    return NULL;
}

/* See description in tapi_jtag_scan.h */
bool
tapi_jtag_chain_matches(const tapi_jtag_chain *chain, const uint32_t *expected,
                        size_t n)
{
    size_t i;

    if (te_vec_size(&chain->taps) != n)
        return false;

    for (i = 0; i < n; i++)
    {
        const tapi_jtag_tap *tap = te_vec_get_immutable(&chain->taps, i);

        if (tap->idcode != expected[i])
            return false;
    }

    return true;
}

/* See description in tapi_jtag_scan.h */
bool
tapi_jtag_chain_has_bad_idcode(const tapi_jtag_chain *chain,
                               const char **which)
{
    const tapi_jtag_tap *tap;

    TE_VEC_FOREACH(&chain->taps, tap)
    {
        if (tap->idcode == 0x00000000 || tap->idcode == 0xFFFFFFFF)
        {
            if (which != NULL)
                *which = tap->name;
            return true;
        }
    }

    return false;
}

/* See description in tapi_jtag_scan.h */
void
tapi_jtag_chain_log(const tapi_jtag_chain *chain)
{
    te_string s = TE_STRING_INIT;
    const tapi_jtag_tap *tap;

    TE_VEC_FOREACH(&chain->taps, tap)
    {
        te_string_append(&s, "  %u %-24s %s idcode 0x%08" PRIx32,
                         tap->index, tap->name, tap->enabled ? "on " : "off",
                         tap->idcode);
        if (tap->expected != 0 && tap->expected != tap->idcode)
            te_string_append(&s, " (expected 0x%08" PRIx32 ")", tap->expected);
        te_string_append(&s, " irlen %u\n", tap->irlen);
    }

    RING("Scan chain, %zu TAP(s):\n%s", te_vec_size(&chain->taps),
         s.ptr != NULL ? s.ptr : "");
    te_string_free(&s);
}
