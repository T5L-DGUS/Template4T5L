# FW protocol host checks

These tests compile the actual `modules/fw_protocol.c` state machine against
fixed-width host types and simulated UART, DGUS VP, page ID and system tick.
They cover event-only snapshots, the current parameter ranges, setting writes,
frame validation, timeout/exception recovery, tick wraparound and the existing
USB update handshake. They do not replace a Keil build or board-level UART tests.

With Node.js and LLVM (`clang`, `llvm-link`, `lli`) on PATH:

```powershell
node tests/run-fw-tests.mjs
```

An existing compiler can be selected without changing PATH:

```powershell
$env:CLANG = 'D:\software\CDKRepo\Toolchain\XTLLVMElfNewlib\V2.4.0\R\bin\clang.exe'
node tests/run-fw-tests.mjs
```

The runner builds LLVM IR into a unique temporary directory, executes it with
LLVM's interpreter and removes the build artifacts. No npm packages are required.
`LLVM_LINK` and `LLI` can override the utilities next to the selected compiler.
A failed test exits with status 1. The `run_tests` function additionally returns
the failed C line number for use from a debugger or a custom runner.
For a native C compiler, the test source also includes `main`:

```sh
cc -std=c99 -Wall -Wextra -Werror -DFW_PROTOCOL_TEST -Itests tests/fw_protocol_test.c modules/fw_protocol.c -o fw-protocol-test
./fw-protocol-test
```
