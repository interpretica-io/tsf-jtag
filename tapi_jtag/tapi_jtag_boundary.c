/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Boundary scan
 *
 * Implementation over UrJTAG (@c jtag), driven by a command script on
 * its standard input.
 */

#define TE_LGR_USER "TAPI JTAG BOUNDARY"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_jtag_boundary.h"
#include "tapi_jtag_internal.h"

/* See description in tapi_jtag_boundary.h */
const tapi_jtag_boundary_opt tapi_jtag_boundary_default_opt = {
    .cable = NULL, .cable_params = NULL, .part_bsdl = NULL, .part_index = 0,
};

/**
 * Build a UrJTAG script: open the cable, detect the chain, load the BSDL
 * onto the part, run @p body, exit. UrJTAG reads commands from stdin, so
 * the whole script is fed through @c sh as one heredoc-free pipeline.
 */
static te_errno
boundary_script(tapi_job_factory_t *factory, const tapi_jtag_boundary_opt *opt,
                const char *body, tapi_jtag_result *result)
{
    te_string script = TE_STRING_INIT;
    const char *args[5];
    int status = -1;
    te_errno rc;

    if (opt->cable == NULL || opt->part_bsdl == NULL)
    {
        ERROR("Boundary scan needs a cable and a BSDL file");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    /*
     * The UrJTAG command script is assembled from positional parameters so
     * nothing in the paths or names is interpreted by the shell: $1 cable,
     * $2 cable params, $3 bsdl, $4 part index, $5 body.
     */
    te_string_append(&script,
        "{ printf 'cable %s %s\\n' \"$1\" \"$2\"; "
        "printf 'detect\\n'; "
        "printf 'bsdl path %s\\n' \"$(dirname \"$3\")\"; "
        "printf 'part %s\\n' \"$4\"; "
        "printf '%s\\n' \"$5\"; "
        "printf 'quit\\n'; } | jtag 2>&1");

    args[0] = opt->cable;
    args[1] = opt->cable_params != NULL ? opt->cable_params : "";
    args[2] = opt->part_bsdl;
    {
        char idx[16];

        snprintf(idx, sizeof(idx), "%u", opt->part_index);
        args[3] = idx;
        args[4] = body;

        tapi_jtag_result_init(result);
        rc = tapi_jtag_sh(factory, "jtag", script.ptr, args, 5,
                          TAPI_JTAG_TIMEOUT_MS, &result->output,
                          &result->output, &status);
    }
    te_string_free(&script);

    if (rc == 0)
    {
        result->status = status;
        result->ok = (status == 0) && result->output.ptr != NULL &&
                     strcasestr(result->output.ptr, "error") == NULL &&
                     strstr(result->output.ptr, "not found") == NULL;
        if (!result->ok)
            ERROR("UrJTAG boundary scan failed: %s",
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    return rc;
}

/* See description in tapi_jtag_boundary.h */
te_errno
tapi_jtag_boundary_run(tapi_job_factory_t *factory,
                       const tapi_jtag_boundary_opt *opt, const char *commands,
                       tapi_jtag_result *result)
{
    return boundary_script(factory, opt, commands, result);
}

/* See description in tapi_jtag_boundary.h */
te_errno
tapi_jtag_boundary_sample(tapi_job_factory_t *factory,
                          const tapi_jtag_boundary_opt *opt,
                          te_string *signals)
{
    tapi_jtag_result result;
    te_errno rc;

    /* SAMPLE captures the pins; "get signal" then reads them all via dump. */
    rc = boundary_script(factory, opt, "instruction SAMPLE\nshift ir\ncapture\ndr", &result);
    if (rc == 0 && result.output.ptr != NULL)
        te_string_append(signals, "%s", result.output.ptr);
    tapi_jtag_result_free(&result);

    return rc;
}

/* See description in tapi_jtag_boundary.h */
te_errno
tapi_jtag_boundary_get_signal(tapi_job_factory_t *factory,
                              const tapi_jtag_boundary_opt *opt,
                              const char *signal, bool *value)
{
    tapi_jtag_result result;
    char *body;
    const char *pos;
    const char *line;
    size_t len;
    te_errno rc;

    *value = false;
    /* SAMPLE/PRELOAD then read the named signal. */
    body = te_string_fmt("instruction SAMPLE\nshift ir\ncapture\ndr\n"
                         "get signal %s", signal);
    rc = boundary_script(factory, opt, body, &result);
    free(body);

    if (rc == 0 && result.ok && result.output.ptr != NULL)
    {
        /* UrJTAG prints "<signal> = 0" or "= 1". */
        for (pos = result.output.ptr;
             (line = tapi_jtag_next_line(&pos, &len)) != NULL; )
        {
            const char *eq = memchr(line, '=', len);

            if (eq != NULL && strstr(line, signal) != NULL)
            {
                const char *p = eq + 1;

                while (p < line + len && *p == ' ')
                    p++;
                *value = (p < line + len && *p == '1');
                break;
            }
        }
    }
    tapi_jtag_result_free(&result);

    return rc;
}

/* See description in tapi_jtag_boundary.h */
te_errno
tapi_jtag_boundary_set_signal(tapi_job_factory_t *factory,
                              const tapi_jtag_boundary_opt *opt,
                              const char *signal, bool value)
{
    tapi_jtag_result result;
    char *body;
    te_errno rc;

    /* EXTEST drives the pins; set the signal, shift it out. */
    body = te_string_fmt("instruction EXTEST\nset signal %s out %d\n"
                         "shift dr", signal, value ? 1 : 0);
    rc = boundary_script(factory, opt, body, &result);
    free(body);

    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    tapi_jtag_result_free(&result);

    return rc;
}
