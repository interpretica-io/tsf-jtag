/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief JTAG and SWD debug access from a Test Agent
 *
 * The core: run helpers, tool detection, the OpenOCD argument builder and
 * the batch runner.
 */

#define TE_LGR_USER "TAPI JTAG"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "tapi_job_opt.h"

#include "tapi_devtool_run.h"

#include "tapi_jtag.h"
#include "tapi_jtag_internal.h"

/** Arguments of a command, as a plain vector of strings. */
typedef struct jtag_cmd_opt {
    size_t n_args;
    const char **args;
} jtag_cmd_opt;

static const tapi_job_opt_bind jtag_cmd_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_ARRAY_PTR(jtag_cmd_opt, n_args, args,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false))
);

/* See description in tapi_jtag_internal.h */
te_errno
tapi_jtag_run_tool(tapi_job_factory_t *factory, const char *name,
                   const char *program, const char **args, size_t n_args,
                   int timeout_ms, te_string *out, te_string *err,
                   int *status)
{
    jtag_cmd_opt opt = { .n_args = n_args, .args = args };
    tapi_devtool_run run = TAPI_DEVTOOL_RUN_INIT;
    tapi_devtool_output output;
    te_errno rc;

    rc = tapi_devtool_run_init(&run, factory, name, program, jtag_cmd_binds,
                               &opt, NULL);
    if (rc != 0)
        return rc;

    rc = tapi_devtool_run_start(&run);
    if (rc == 0)
        rc = tapi_devtool_run_wait(&run, timeout_ms);

    if (rc == 0)
    {
        tapi_devtool_run_get_output(&run, &output);
        if (out != NULL)
            te_string_append(out, "%s", output.out);
        if (err != NULL)
            te_string_append(err, "%s", output.err);
        if (status != NULL)
            *status = (output.status.type == TAPI_JOB_STATUS_EXITED) ?
                      output.status.value : -1;
    }

    tapi_devtool_run_fini(&run);

    return rc;
}

/* See description in tapi_jtag_internal.h */
te_errno
tapi_jtag_sh(tapi_job_factory_t *factory, const char *name,
             const char *script, const char **args, size_t n_args,
             int timeout_ms, te_string *out, te_string *err, int *status)
{
    const char **argv;
    size_t i;
    te_errno rc;

    argv = TE_ALLOC((n_args + 3) * sizeof(*argv));
    argv[0] = "-c";
    argv[1] = script;
    argv[2] = "sh";
    for (i = 0; i < n_args; i++)
        argv[i + 3] = args[i];

    rc = tapi_jtag_run_tool(factory, name, "sh", argv, n_args + 3, timeout_ms,
                            out, err, status);

    free(argv);

    return rc;
}

/* See description in tapi_jtag_internal.h */
te_errno
tapi_jtag_have_tool(tapi_job_factory_t *factory, const char *program,
                    bool *present)
{
    const char *args[] = { program };
    int status;
    te_errno rc;

    rc = tapi_jtag_sh(factory, "which", "command -v \"$1\" >/dev/null", args,
                      1, TAPI_JTAG_TIMEOUT_MS, NULL, NULL, &status);
    if (rc == 0)
        *present = (status == 0);

    return rc;
}

/* See description in tapi_jtag_internal.h */
const char *
tapi_jtag_next_line(const char **pos, size_t *len)
{
    const char *line = *pos;
    const char *eol;

    if (line == NULL || *line == '\0')
    {
        *pos = NULL;
        return NULL;
    }

    eol = strchr(line, '\n');
    if (eol != NULL)
    {
        *len = (size_t)(eol - line);
        *pos = eol + 1;
    }
    else
    {
        *len = strlen(line);
        *pos = line + *len;
    }

    return line;
}

/* See description in tapi_jtag.h */
const tapi_jtag_adapter tapi_jtag_adapter_default = {
    .configs = NULL, .n_configs = 0, .pre_commands = NULL,
    .n_pre_commands = 0, .search_dir = NULL,
    .transport = TAPI_JTAG_TRANSPORT_DEFAULT, .speed_khz = 0,
};

/* See description in tapi_jtag_internal.h */
void
tapi_jtag_ocd_args(const tapi_jtag_adapter *adapter, const char **commands,
                   size_t n_commands, te_vec *args, te_vec *pool)
{
    const char *opt;
    size_t i;

    if (adapter->search_dir != NULL)
    {
        opt = "-s";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, adapter->search_dir);
    }

    /*
     * The transport and the adapter speed must be selected after the
     * interface config but before the target is examined; they go in as
     * -c commands ahead of the config files, which OpenOCD evaluates in
     * order with the config files last only if given with -f. To keep the
     * order right we emit the interface configs, then transport/speed,
     * then the rest. Callers pass the interface config first.
     */
    for (i = 0; i < adapter->n_configs; i++)
    {
        opt = "-f";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, adapter->configs[i]);
    }

    if (adapter->transport != TAPI_JTAG_TRANSPORT_DEFAULT)
    {
        const char *t = (adapter->transport == TAPI_JTAG_TRANSPORT_SWD) ?
                        "transport select swd" : "transport select jtag";

        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, t);
    }
    if (adapter->speed_khz != 0)
    {
        char *s = te_string_fmt("adapter speed %u", adapter->speed_khz);
        const char *cs = s;

        TE_VEC_APPEND(pool, s);
        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, cs);
    }

    for (i = 0; i < adapter->n_pre_commands; i++)
    {
        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, adapter->pre_commands[i]);
    }
    for (i = 0; i < n_commands; i++)
    {
        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, commands[i]);
    }
}

/* See description in tapi_jtag.h */
te_errno
tapi_jtag_tools_probe(tapi_job_factory_t *factory, tapi_jtag_tools *tools)
{
    static const struct {
        const char *program;
        size_t offset;
    } probe[] = {
        { "openocd", offsetof(tapi_jtag_tools, openocd) },
        { "jtag", offsetof(tapi_jtag_tools, urjtag) },
        { "openFPGALoader", offsetof(tapi_jtag_tools, openfpgaloader) },
    };
    size_t i;
    te_errno rc;

    memset(tools, 0, sizeof(*tools));

    for (i = 0; i < TE_ARRAY_LEN(probe); i++)
    {
        bool present = false;

        rc = tapi_jtag_have_tool(factory, probe[i].program, &present);
        if (rc != 0)
            return rc;
        *(bool *)((char *)tools + probe[i].offset) = present;
    }

    return 0;
}

/* See description in tapi_jtag.h */
void
tapi_jtag_result_init(tapi_jtag_result *result)
{
    result->ok = false;
    result->status = -1;
    result->output = (te_string)TE_STRING_INIT;
}

/* See description in tapi_jtag.h */
void
tapi_jtag_result_free(tapi_jtag_result *result)
{
    te_string_free(&result->output);
}

/* See description in tapi_jtag.h */
te_errno
tapi_jtag_run(tapi_job_factory_t *factory, const tapi_jtag_adapter *adapter,
              const char **commands, size_t n_commands,
              tapi_jtag_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);
    const char **all;
    const char *shutdown = "shutdown";
    size_t i;
    int status = -1;
    te_errno rc;

    tapi_jtag_result_init(result);

    /* Always finish with shutdown, or openocd waits for a connection. */
    all = TE_ALLOC((n_commands + 1) * sizeof(*all));
    for (i = 0; i < n_commands; i++)
        all[i] = commands[i];
    all[n_commands] = shutdown;

    tapi_jtag_ocd_args(adapter, all, n_commands + 1, &args, &pool);
    free(all);

    rc = tapi_jtag_run_tool(factory, "openocd", "openocd",
                            (const char **)te_vec_get_mutable(&args, 0),
                            te_vec_size(&args), TAPI_JTAG_TIMEOUT_MS,
                            &result->output, &result->output, &status);
    te_vec_free(&args);
    te_vec_deep_free(&pool);
    if (rc != 0)
        return rc;

    result->status = status;
    /*
     * OpenOCD can exit 0 having printed an error, so the output is checked
     * too: a lost target or a failed command says so in words.
     */
    result->ok = (status == 0) && result->output.ptr != NULL &&
                 strstr(result->output.ptr, "Error:") == NULL;
    if (!result->ok)
        ERROR("openocd failed (exit %d): %s", status,
              result->output.ptr != NULL ? result->output.ptr : "");

    return 0;
}
