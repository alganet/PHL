# Tests — Repository Structure and Guidelines

This directory contains the project's test suites. Tests are organized by scope and purpose to make it easier to find, add, and run focused checks.

Quick layout
```
tests/
├─ phpt.php
├─ phptrunner/
├─ ph7/
│  ├─ 001-smoke/          # High-level smoke tests (small and representative)
│  └─ 002-engine/         # Engine-level tests (internal API & builtin tests)
│     ├─ array/           # Tests that exercise PHL's hashmap implementation
│     ├─ constants/       # Per-constant tests (each constant gets a file)
│     ├─ function/        # Per-function tests (each function gets a folder)
│     │─ oo/              # Object-oriented tests for classes, interfaces, etc
│     │  ├─ addslashes/addslashes.phpt
│     │  ├─ file_put_contents/file_put_contents.phpt
│     │  ├─ file_put_contents/file_put_contents_zend_win.phpt
│     │  └─ ...etc.
```

Conventions & style
- Tests are PHPT-format files with sections such as `--TEST--`, `--FILE--`, `--EXPECT--`/`--EXPECTF--` and (optionally) `--CLEAN--`.
- All test files must include the `--CREDITS--` section at the top for SPDX compliance.
- Prefer focused tests: each PHPT should exercise one behavior of one function or feature. Avoid huge grouped tests asserting many unrelated behaviors.
- All test files end with a single trailing new line.
- Naming:
  - Use a clear directory name matching the runtime function or subsystem (e.g., `function/addslashes/addslashes.phpt`).
  - For parser and engine infra tests that aren’t function-level, use descriptive group names (e.g., `function/xml_parser_create/xml_declaration_valid.phpt`).
  - Individual functions may require one or more specialized PHPT files; place them in the function's folder (for example, `function/addslashes/addslashes.phpt` and `function/addslashes/addslashes_multibyte.phpt`).

How to add a test
1. Create a directory under the correct scope: `tests/ph7/002-engine/function/<function>/`.
2. Add a single PHPT file named `<function>.phpt` with the minimal `--TEST--`, `--FILE--` and `--EXPECT--` sections.
3. Run compatibility and coverage checks (see below).

Minimal PHPT example
```
--TEST--
addslashes escapes single quote
--FILE--
<?php
echo addslashes("John's") . "\n";
?>
--EXPECT--
John\'s
```

Running & verifying tests
- Run the compatibility suite (PHL + PHP):
  - make test-compat
- Generate coverage information:
  - make coverage
  - Coverage is printed by file under `build/*/coverage`.

Notes and tips
- Keep tests minimal: one core behavior per test file. Prefer many focused tests to a few large ones.
- When you move or rename tests, update expected outputs that depend on basename (for example, `basename($errfile)` used by error handler tests).
- If a test must differ per runtime, prefer to create a complementary test under the same function dir and mark differences with `--EXPECTF--` patterns.

Best Practices
- Use `--CREDITS--` on new PHPT files with SPDX metadata.
- Add a `--CLEAN--` section to unset variables, close resources and remove temporary files so tests do not pollute the runner context.
- Use `--SKIPIF--` to skip tests that depend on features not available in the current runtime (e.g. PH7-only constants or functions).
- Prefer `--EXPECTF--` or numeric formatting (e.g., `sprintf`) when runtime differences (precision, platform behavior) may cause failures.
- Use `tempnam(sys_get_temp_dir(), 'ph7_')` for temporary files and clean them in `--CLEAN--`.
- For Windows vs Unix differences, use `PHP_OS` to branch or skip.

Maintainers
- If you’re not sure where to put a test — ask in the project's issue tracker or reach out to the maintainers.

That's it — happy testing! ✅
