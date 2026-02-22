# Copilot instructions for PHL

## Big picture
- PHL is an embeddable PHP engine in C with a thin CLI wrapper.
- Main subsystems are split by directory (not amalgamated):
  - `src/sx/`: low-level utility/runtime layer (memory, strings, hash, lexer helpers, XML/ZIP helpers).
  - `src/ph7/`: PHP engine (public API, lexer/parser/compiler/VM, builtins, OO, VFS).
  - `src/phl/`: standalone `phl` interpreter (`main()` and CLI behavior).
- Typical execution flow: `ph7_init()` → `ph7_compile_v2()`/`ph7_compile_file()` → `ph7_vm_config()` (output/errors/argv) → `ph7_vm_exec()` (see `src/phl/phl.c`, `src/ph7/api.c`).

## Build and run workflows
- Default build: `make` (equivalent to `make MODE=full`).
- Build modes are intentional and used in CI:
  - `MODE=full`: standard engine (`PH7_ENABLE_MATH_FUNC`, threads).
  - `MODE=tiny`: heavily reduced footprint (`PH7_DISABLE_BUILTIN_FUNC`, `PH7_DISABLE_DISK_IO`, etc.).
  - `MODE=coverage`: instrumentation build used by `coverage-report`.
- Output binary path is mode+target specific: `build/<target>/<mode>/phl`.
- Linux/macOS use GNU make includes (`build-aux/gnu.mk`, `build-aux/rules.mk`); Windows uses NMake via the polyglot `Makefile` and `build-aux/nmake.mk`.
- For Windows from WSL, use wrapper: `sh build-aux/dev.sh make test` (invokes `build-aux/dev.bat nmake ...`).

## Testing workflows
- Primary targets:
  - `make test` (smoke + integration against built `phl`).
  - `make test-compat` (runs suites against system `php` first, then `phl`).
  - `make coverage-report` (generates `build/<target>/coverage/coverage.info`).
- Test runner is `tests/phpt.php`; make targets pass `--target-dir` and `--target-executable`.
- Useful focused run during development:
  - `build/<target>/full/phl tests/phpt.php --target-executable build/<target>/full/phl --target-dir tests/ph7/001-smoke --filter <prefix>`
- `tests/phpt.php` intentionally marks some PHPT sections as TODO/unimplemented (`expectregex`, `post`, `ini`, etc.); do not treat these as regressions.

## Project-specific coding conventions
- Follow `.editorconfig`: tabs in `*.c`/`*.h`, LF endings; PHP test tooling uses 4 spaces.
- Keep ANSI C89/C90 compatibility (avoid introducing C99-only constructs unless existing file already does).
- Preserve existing SPDX header style in new source files.
- Keep changes localized by subsystem:
  - language parse/compile behavior: `src/ph7/lex.c`, `parse.c`, `compile.c`, `vm.c`.
  - builtin function behavior: `src/ph7/builtin.c` (+ matching tests under `tests/ph7/**/function/`).
  - constants: `src/ph7/constant.c` (+ `tests/ph7/**/constants/`).
  - arrays/hashmap/object internals: `src/ph7/hashmap.c`, `memobj.c`, `oo.c`.

## Integration points and extension hooks
- Public embedding API surface is `src/ph7/ph7.h`; internals are in `src/ph7/ph7int.h`.
- Engine-level configurability is centralized in `ph7_lib_config()`/`ph7_config()` (`src/ph7/api.c`): VFS, memory allocator, mutex subsystem, error consumers.
- Use examples as canonical host-integration patterns: `examples/ph7_intro.c`, `examples/ph7_func_intro.c`, `examples/ph7_const_intro.c`, `examples/ph7_cgi.c`.
