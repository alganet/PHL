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

## The php.ini matrix

Both corpora, `test-compat` and `tests/vendor` all run the one combination a
stock CLI starts with — `display_errors` off, `log_errors` on — and the runner
merges the child's stderr into its stdout unless the test declares
`--EXPECT_STDERR--`. That is the single setting in which a diagnostic on the
wrong stream, in the wrong shape, or missing a whole copy still reads as
correct: a compile diagnostic that ignored both directives outright passed 4784
rows.

`--ini name=value` (repeatable) applies a directive to every child, beneath any
the test's own `--INI--` section sets, so a whole corpus run can be re-taken at
another setting. `tests/ini_matrix.php` — `make test-ini-matrix` — is what does
that: the integration corpus at all four combinations in both engines, with the
*verdicts* compared rather than the bytes, since at `display_errors=1` hundreds
of tests legitimately fail in both. A test skipped by either engine is not
comparable and is left out. The oracle's four passes depend on the corpus and on
php, never on the engine under test, so they are cached under a fingerprint of
both.

## Per-diagnostic notes

- `002-fail_test.diag` — output mismatch; reports as a failure by design.
- `004-unimplemented_test.diag` — carries a `--GET--` section. The CGI-shaped
  sections (`--POST--`/`--POST_RAW--`/`--GET--`/`--COOKIE--`) are unimplemented
  **by design**, not pending: PHL is CLI + `-S` only. A test carrying one is
  reported as a failure rather than silently passing, so this fails by design.
- `007-handler_format_test.diag` — verifies the in-process `handle_error`
  normalization (`Error [%d]: …`). It only passes **in-process**; under
  `--target-executable` the engine emits its native warning format, so it fails
  there. An in-process-mode diagnostic.
- `008-args_test.diag` / `009-stdin_test.diag` — `--ARGS--` (appended to the
  child's argv) and `--STDIN--` (redirected into the child). Both shape the child
  *invocation*, so like `--ENV--`/`--INI--` they need `--target-executable`; the
  in-process runner shares its own argv and stdin, so those tests SKIP there with
  a written reason rather than running with the section silently dropped.
- `010-expectregex_test.diag` — `--EXPECTREGEX--`; the body is a delimiter-less
  pattern matched against the whole output. Works in both modes.
- `011-exit_test.diag` — calls `die()`. In the default in-process run it triggers
  the `Bail out!` guard and aborts the run, so it is intentionally LAST — keep it
  last when adding diagnostics. Under `--target-executable` it runs in a child
  process and passes.
