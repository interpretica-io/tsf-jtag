/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Playing SVF and XSVF vector files
 *
 * Implementation over OpenOCD's svf and xsvf commands.
 */

#define TE_LGR_USER "TAPI JTAG SVF"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_jtag_svf.h"
#include "tapi_jtag_internal.h"

/** Play a vector file with the given OpenOCD verb (@c svf or @c xsvf). */
static te_errno
svf_play(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
         const char *verb, const char *path, bool *ok)
{
    tapi_jtag_result result;
    const char *commands[2];
    char *cmd;
    te_errno rc;

    *ok = false;
    commands[0] = "init";
    cmd = te_string_fmt("%s \"%s\"", verb, path);
    commands[1] = cmd;

    rc = tapi_jtag_run(factory, adapter, commands, 2, &result);
    if (rc == 0)
    {
        /*
         * OpenOCD prints "svf processed N commands with M errors" and
         * "Time used: ..."; success is exit 0, no "Error:", and, when it
         * says so, zero errors.
         */
        *ok = result.ok &&
              (result.output.ptr == NULL ||
               strstr(result.output.ptr, "with 0 errors") != NULL ||
               (strstr(result.output.ptr, "errors") == NULL &&
                strstr(result.output.ptr, "failed") == NULL));
        if (!*ok)
            ERROR("%s of %s did not verify: %s", verb, path,
                  result.output.ptr != NULL ? result.output.ptr : "");
    }
    tapi_jtag_result_free(&result);
    free(cmd);

    return rc;
}

/* See description in tapi_jtag_svf.h */
te_errno
tapi_jtag_svf_play(tapi_job_factory_t *factory,
                   const tapi_jtag_adapter *adapter, const char *path,
                   bool *ok)
{
    return svf_play(factory, adapter, "svf", path, ok);
}

/* See description in tapi_jtag_svf.h */
te_errno
tapi_jtag_xsvf_play(tapi_job_factory_t *factory,
                    const tapi_jtag_adapter *adapter, const char *path,
                    bool *ok)
{
    /* OpenOCD's xsvf takes a tap/target name before the file; "-" plays
     * against the whole chain as recorded. */
    tapi_jtag_result result;
    const char *commands[2];
    char *cmd;
    te_errno rc;

    *ok = false;
    commands[0] = "init";
    cmd = te_string_fmt("xsvf -tap - \"%s\"", path);
    commands[1] = cmd;
    /* Fall back to the plain form if -tap is not accepted. */
    rc = tapi_jtag_run(factory, adapter, commands, 2, &result);
    if (rc == 0 && !result.ok)
    {
        tapi_jtag_result_free(&result);
        free(cmd);
        return svf_play(factory, adapter, "xsvf", path, ok);
    }
    if (rc == 0)
        *ok = result.ok;
    tapi_jtag_result_free(&result);
    free(cmd);

    return rc;
}
