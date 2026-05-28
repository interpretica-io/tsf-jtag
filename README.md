# tsf-jtag

Reaching the JTAG/SWD debug and boundary-scan interface of a device under
test from a Test Agent — packaged as an external Test Environment (TE)
repository and consumed with the `TE_EXT_REPO` builder directive.

Library:

- `tapi_jtag` — engine-side TAPIs, built as a shared library:
  - `tapi_jtag` — how a probe and the chain behind it are described,
    which tools the agent has, and running a batch of commands;
  - `tapi_jtag_scan` — the scan chain: the TAPs on it, their IDCODEs, and
    whether they are the ones expected;
  - `tapi_jtag_debug` — a core on the chain: halt, resume, reset, step,
    and read and write its memory and registers;
  - `tapi_jtag_svf` — playing an SVF or XSVF vector file onto the chain;
  - `tapi_jtag_boundary` — boundary scan: sampling and driving pins for
    board bring-up without any firmware.

It builds on
[tsf-devtool](https://github.com/interpretica-io/tsf-devtool), for running
a tool on an agent and capturing what it printed. Almost everything goes
through `openocd`, the tool every probe speaks; the boundary scan goes
through `jtag` (UrJTAG).

## Usage

Declare the repositories in an external libraries catalog (e.g.
`conf/external.yml` in the test suite) and pass it to
`dispatcher.sh --external=external.yml`:

```yaml
repositories:
  - name: tsf_devtool
    url: https://github.com/interpretica-io/tsf-devtool.git
    ref: <tag>
    libs:
      - tapi_devtool
  - name: tsf_jtag
    url: https://github.com/interpretica-io/tsf-jtag.git
    ref: <tag>
    libs:
      - tapi_jtag
```

Bind them to the engine platform in `builder.conf`:

```
TE_EXT_REPO_USE([tsf_devtool], [], [tapi_devtool])
TE_EXT_REPO_USE([tsf_jtag], [], [tapi_jtag])
```

and add `tapi_jtag` to the `te_libs` list of the suite's `meson.build`.

Requires TE with `TE_EXT_REPO` support, and an **RPC** job factory
(`ta_rpcprovider` on the agent).

## Where the tool runs

The probe is wired to a host, and OpenOCD runs there — the agent behind
the job factory, which may be the engine host itself. An adapter is
described by the OpenOCD config files (the interface, then the target),
optionally a transport (JTAG or SWD) and an adapter clock:

```c
tapi_jtag_adapter adapter = tapi_jtag_adapter_default;
static const char *cfg[] = { "interface/stlink.cfg", "target/stm32f4x.cfg" };

adapter.configs = cfg;
adapter.n_configs = 2;
adapter.transport = TAPI_JTAG_TRANSPORT_SWD;
```

## The scan chain

The first thing a board bring-up checks: that every part is present, in
order, and reads the ID it should.

```c
tapi_jtag_chain chain;
static const uint32_t expected[] = { 0x06413041, 0x4ba00477 };

CHECK_RC(tapi_jtag_scan(factory, &adapter, &chain));
tapi_jtag_chain_log(&chain);
if (tapi_jtag_chain_has_bad_idcode(&chain, NULL))
    TEST_VERDICT("A TAP read an all-ones/all-zeros IDCODE (reset or open)");
if (!tapi_jtag_chain_matches(&chain, expected, TE_ARRAY_LEN(expected)))
    TEST_VERDICT("The scan chain is not the board that was expected");
tapi_jtag_chain_free(&chain);
```

An all-ones or all-zeros IDCODE means a part held in reset or unpowered,
or a broken TDI/TDO trace — `tapi_jtag_chain_has_bad_idcode()` catches it.

## Core debug

Stop a CPU and look inside it — read a boot flag from RAM, check a
peripheral register came up with its reset value, confirm the reset
vector:

```c
uint32_t magic;
uint64_t pc;

CHECK_RC(tapi_jtag_reset(factory, &adapter, TAPI_JTAG_RESET_HALT));
CHECK_RC(tapi_jtag_read_word(factory, &adapter, NULL, 0x20000000, &magic));
CHECK_RC(tapi_jtag_read_reg(factory, &adapter, NULL, "pc", &pc));
CHECK_RC(tapi_jtag_resume(factory, &adapter, NULL));
```

`tapi_jtag_read_mem()` reads a run of words; `tapi_jtag_write_word()` and
`tapi_jtag_write_reg()` write. A sequence of operations can go in one
OpenOCD session with `tapi_jtag_debug_session()` to avoid re-attaching.
Halting stops the device — a test that halts should resume or reset it.

## SVF and boundary scan

`tapi_jtag_svf_play()` plays an SVF (or `tapi_jtag_xsvf_play()` an XSVF)
onto the chain, failing if a shifted-out value does not match what the
file expected — the portable way a CPLD is programmed or a fixed JTAG
procedure is verified.

`tapi_jtag_boundary` reaches the pins of a part with no firmware running,
through UrJTAG and the part's BSDL: `tapi_jtag_boundary_sample()` reads
every pin, `tapi_jtag_boundary_get_signal()` / `_set_signal()` read and
drive one — for testing a trace is not open or shorted before the board
can boot.

## Scope

The probe drives a device electrically and can hold it in reset or halt
it. Everything here points at the board the suite's own configuration
names, over the probe the agent owns; that is the engagement it belongs
to.
