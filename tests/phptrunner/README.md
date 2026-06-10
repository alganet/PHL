# phpt.php diagnostics tests

These are not part of the PHL test suite, and only exist to diagnose
the `phpt.php` test runner. As so, they use the `diag` extension that
the runner does not pick up by default.

You can run those diagnostics using:

```sh
phl tests/phpt.php --target-dir tests/phptrunner --file-extension diag
```

## The two runner modes

- **In-process (default, no `--target-executable`):** each test's `--FILE--`
  is `include()`d in the runner's own process. This is fast, so the smoke
  corpus uses it — but an in-process test must never call `exit`/`die` or
  otherwise pollute the interpreter. A test that kills the interpreter is
  caught by a shutdown guard that prints a TAP `Bail out!` and exits nonzero,
  instead of silently truncating the run.
- **External (`--target-executable <bin>`):** each test runs in its own child
  process. Required for tests that exit/die or mutate global interpreter state;
  the integration corpus runs this way.

## Per-diagnostic notes

- `002-fail_test.diag` — output mismatch; reports as a failure by design.
- `004-unimplemented_test.diag` — unsupported section; reports as a failure by design.
- `007-handler_format_test.diag` — verifies the in-process `handle_error`
  normalization (`Error [%d]: …`). It only passes **in-process**; under
  `--target-executable` the engine emits its native warning format, so it fails
  there. An in-process-mode diagnostic.
- `008-exit_test.diag` — calls `die()`. In the default in-process run it triggers
  the `Bail out!` guard and aborts the run, so it is intentionally LAST. Under
  `--target-executable` it runs in a child process and passes.
