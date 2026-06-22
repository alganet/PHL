# PHL Gap Analysis vs Official PHP

This document tracks the gaps between **PHL** (a lightweight, embeddable, single-binary
PHP built on the PH7 engine) and official PHP. It is the project roadmap: what works,
what doesn't, and what to build next.

As of 12 Jun 2026 it also tracks **engine performance and embedding ergonomics** (§3.8),
surfaced by the first hardware profile (PHL on ESP32-S3) but platform-independent — the
same issues reproduce on the host and the fixes benefit the CLI and `-S` server too.
Port-specific follow-ups live in `ESP32.md`; the engine prerequisites are here.

## How to read this

- **§1 Status dashboard** — one-line status for every tracked feature. Start here.
- **§2 Recently completed** — what's been finished, so progress is visible.
- **§3 Remaining gaps** — detail (evidence, scope, dependencies, effort) for the open items.
- **§4 Dependency map** — which features unblock which; order follows this, not raw effort.
- **§5 Roadmap** — the recommended build order.
- **§6 Scope policy** — features deliberately out of scope, pending ratification.
- **§7 Verification** — a re-runnable probe so this document can be re-checked, not asserted.

**Effort rubric** (implementation + tests + edge cases):
- **S** ≤1 day — single function/opcode/keyword, straightforward tests.
- **M** 2–5 days — multiple layers (lexer + parser + compiler + VM) or non-trivial semantics.
- **L** 1–2 weeks — cross-cutting feature (keyword set, class family, subsystem).
- **XL** 3+ weeks — major subsystem.

**Status legend:** ✅ Done · 🟡 Partial/Broken · ❌ Missing · ⛔ Out-of-scope (proposed).

> **Analysed build:** PHL 2.1.4 (cli), HEAD `3c2344e`, re-verified 10 Jun 2026. Binary:
> `build/arm64-apple-darwin/full/phl`. Every status below was re-probed against this build
> (§7); regenerate rather than trusting these numbers after future work.
>
> **Live metrics (probe §7):** 490 internal functions · 27 declared classes ·
> 11 declared interfaces · 2769 `.phpt` files. Official PHP exposes ~3000 functions.
>
> **Suite snapshot (direct-external mode, §7):** 2745 ok · 24 not ok · 17 skip of 2769.
> All 24 failures are one class — error-output format coupling (§3.7). The runner keeps
> its fast in-process default (smoke ~4s); a silent truncation is now impossible —
> `exit`/`die` unwinds the VM properly (engine fix, §2) and a shutdown-time abort guard
> turns any interpreter-killing test into a loud TAP `Bail out!` + nonzero exit.

---

## 1. Status dashboard

### Language / parser
| Feature | PHP | Status | Evidence |
|---|---|---|---|
| `**` / `**=` exponentiation | 5.6 | ✅ | `parse.c` `EXPR_OP_POW`; `2**10`→1024 |
| Nested short-array literals `[["a"=>1]]` | 5.4 | ✅ | `compile.c` `GenStateCompileArrayBody` |
| Array unpack with string keys `[...$a]` | 8.1 | ✅ | `PH7_OP_FLAG_SPREAD`, `PH7_HashmapMerge`; Traversable spread ✅ via `PH7_VmIteratorWalk` (§2) |
| Perl-style string increment `"a"++` | 4 | ✅ | `memobj.c` `PH7_MemObjStringIncrement` |
| `iterable` pseudo-type | 7.1 | ✅ | param ✅; property ✅ (array\|Traversable enforced, §2) |
| `true` / `false` as types | 8.2 | ✅ | param ✅; return enforcement ✅ (value-equality check, §2) |
| `null` as standalone type | 8.2 | ✅ | accepted + null-only enforced on returns/properties (§2); `?null` still rejected like PHP |
| `yield from` | 7.0 | ❌ | `Invalid array name`; `PH7_CompileYield` only bare/keyed |
| Keyed list destructuring `["k"=>$v]=…` | 7.1 | ❌ | `Unexpected token '=>'` (both `list()` and `[]`) |
| First-class callable `f(...)` | 8.1 | ❌ | `strlen(...)` → `Invalid function name` warning, NULL |
| Intersection types `A&B` | 8.1 | ❌ | `Invalid argument name`; only union `\|` in `GenStateParseUnionTypeDecl` (`compile.c:445`) |
| DNF types `(A&B)\|C` | 8.2 | ❌ | no parenthesised type groups |
| Enums (pure & backed) | 8.1 | ❌ | no `enum` keyword (`UnitEnum`/`BackedEnum` ifaces exist, `vm.c:1079`) |
| `readonly` property / class | 8.1/8.2 | ❌ | `Unexpected token 'readonly'` |
| Typed class constants `const int X` | 8.3 | ✅ | parsed + strict enforcement (int→float widening only); mismatch is a PHP-exact definition-time fatal (§2) |
| `final` constants | 8.1 | ✅ | `final const`/`public final const` parse; override ban in `PH7_ClassInherit` names the declaring class (§2) |
| Anonymous classes `new class {}` | 7.0 | ❌ | `Unexpected token 'function'` |
| Attributes `#[Attr]` | 8.0 | 🟡 | balanced `#[ … ]` group skipped as inert trivia (`lex.c`, §2) — code around it is safe; no storage/reflection yet |

### Object semantics / magic methods
| Feature | PHP | Status | Evidence |
|---|---|---|---|
| `__invoke` dispatch | 5.3 | ✅ | wired into call_user_func/usort/array_map; exceptions unwind (§2) |
| `__sleep`/`__wakeup` | 5 | ❌ | no serialize hook (`serialize` aliased to JSON `vm.c:15350`) |
| `__serialize`/`__unserialize` | 7.4 | ❌ | as above |
| `__set_state` (var_export) | 5.1 | ❌ | `var_export($obj)` emits `Object(C){…}`, no round-trip |
| `__debugInfo` (var_dump) | 5.6 | ❌ | not consulted |
| Override signature checks (incl. covariance) | 7.4 | 🟡 | **no check at all**: covariant overrides "work", but invalid overrides (`int`→`string` return, narrowed params) are silently accepted too (§3.3) |

### Built-in classes / interfaces
| Feature | PHP | Status | Evidence |
|---|---|---|---|
| SPL interfaces (ArrayAccess, Countable, Stringable, Traversable, UnitEnum, BackedEnum) | 5/8 | ✅ | `vm.c:1066-1084`; dispatch verified (`$obj[$k]`, `count($obj)`, auto-`Stringable`) |
| `Closure` class (`bind`/`fromCallable`/`call`) | 5.3+ | ❌ | `class_exists("Closure")`→false |
| `JsonSerializable` | 5.4 | ✅ | `vm.c:1079` (embedded iface); `json_encode` calls `jsonSerialize()` (`vm_json.c`) |
| SPL exceptions (RuntimeException, LogicException, …13) | 5.1 | ✅ | embedded PHP tree (`vm.c`); PHP-exact hierarchy, catch-by-base verified (§2) |
| SPL data structures (SplStack, SplQueue, SplObjectStorage, SplFixedArray, …) | 5.3 | ❌ | not declared |
| SPL iterators (ArrayIterator, ArrayObject, RecursiveIteratorIterator, …) | 5.x | ❌ | not declared |
| DateTime family (DateTime, DateTimeImmutable, DateInterval, DateTimeZone) | 5.2+ | ❌ | not declared |
| `WeakReference` / `WeakMap` | 7.4/8.0 | ❌ | not declared |
| Reflection API (ReflectionClass, …) | 5 | ❌ | not declared |
| PDO + drivers | 5.1 | ❌ | not declared |

### Standard library
| Feature | PHP | Status | Evidence |
|---|---|---|---|
| `str_contains`/`str_starts_with`/`str_ends_with` | 8.0 | ✅ | `builtin.c:6907-6909` |
| `random_int` / `random_bytes` | 7.0 | ✅ | `vm.c:12056+` (OS CSPRNG) |
| `array_column`/`array_is_list`/`array_find*`/`array_any`/`array_all` | 8.1-8.4 | ✅ | `hashmap.c` `aHashmapFunc[]`; find-family callback gets `($value,$key)` |
| `hash` family (`hash`, `hash_hmac`, `hash_equals`, SHA-256/512) | 5.1 | 🟡 | only `md5`/`sha1`/`crc32` (`builtin.c:6923-6925`) |
| `password_*` | 5.5 | ❌ | undefined (needs hash family) |
| `iterator_to_array`/`iterator_apply`/`iterator_count` | 5.1 | ✅ | `hashmap.c` via `PH7_VmIteratorWalk` (§2) |
| `json_last_error_msg` / `json_validate` | 5.5/8.3 | ✅ | `vm_json.c`; `json_validate` reuses the decode pipeline (§2) |
| `filter_var` / `filter_input` | 5.2 | ❌ | undefined |
| `mb_*` multibyte family | 4.6+ | 🟡 | only `mb_strtolower`/`mb_strtoupper` aliases (`builtin.c:6888-6890`) |
| Sessions (`session_start`, …, `$_SESSION`) | 4 | 🟡 | superglobal infra exists; no functions |
| INI API (`ini_get`/`ini_set`/…) | 4 | ❌ | only `parse_ini_*`; no engine config store |

### Engine / CLI / SAPI
| Feature | PHP | Status | Evidence |
|---|---|---|---|
| `spl_autoload_register` | 5.1 | ✅ | `vm.c:14982`, registered `vm.c:15240` |
| `PHP_BINARY` | 5.4 | ✅ | registered from `phl.c` via `ph7_create_constant`; absolute path (realpath / GetModuleFileNameA). Not yet defined in `-S` server VMs (follow-up) |
| `phpversion()` | 4 | ✅ | `vm.c` `vm_builtin_phpversion`; no-arg → `PHP_COMPAT_VERSION` ("8.5.0"), extension arg → NULL |
| `PHP_VERSION` / `PHP_*_VERSION` / `PHP_VERSION_ID` | 5.2 | ✅ | `constant.c`; `PHP_COMPAT_*` macros (`ph7.h`); `PHP_VERSION_ID`=80500 |
| `php_sapi_name()` | 4.3 | ✅ | `vm.c` `vm_builtin_php_sapi_name`; `"cli"` / `"cli-server"` (via `bHttpContext`) |
| CLI `-r`/`-b`/`-S`/`-t`/`-v`/`--help` | — | ✅ | `phl.c:145-230` |
| CLI `-l` lint / `-i` info / `-a` REPL / `-m` modules | — | ❌ | fall through to Help (re-verified: `-l file` prints usage) |
| CLI `$_SERVER` population | — | ❌ | `$_SERVER["SCRIPT_NAME"]` unset; only `$argv`/`$argc`/`$_ENV` |
| `-S` dev server concurrency | — | 🟡 | single-threaded (`server.c:14`) |
| `php.ini` config subsystem | 4 | ❌ | no parser/store/`-d` |
| Stream wrappers / sockets | 4+ | ❌ | internal `net.c` only |
| Opcache / JIT | 7.0/8.0 | ⛔ | proposed out-of-scope (§6) |
| `dl()` dynamic extensions | 4 | ⛔ | proposed out-of-scope (§6) |
| FPM / FastCGI / mod_php | — | ⛔ | proposed out-of-scope (§6) |

### Engine performance & embedding (§3.8)
| Concern | Status | Evidence |
|---|---|---|
| String concat `.=` amortized-O(1) growth | ✅ | `OP_CAT_STORE` in-place append fast path (§2); host n=80k `.=` 11.57 s → 0.01 s |
| Allocation failure surfaced to PHP | ✅ | OOM raises a non-catchable fatal via `PH7_VmMemoryError` (str/array builtins, concat); broader audit deferred (§2) |
| VM reuse across requests (`ph7_vm_reset`) | ✅ | full per-exec reset (globals, superglobals, statics, closures, OB, refs); definitions/constants persist; compile-once/execute-many. `-S` server reuses VMs (§2) |
| `microtime(true)`/`gettimeofday` µs resolution | ✅ | float branch returns `sec+usec/1e6`; `microtime()` string is PHP-exact `0.dddddddd sec`; `PH7_CONFIG_CLOCK` embedder hook (§2) |
| `preg_match` `$matches` capture | ✅ | populated incl. for undefined vars (compiler auto-vivifies by-ref builtin out-params, §2) |
| Iterative VM (small native stack/frame) | ❌ | ~800 B C-stack per PHP frame (§6, deferred); but the depth cap now fails *gracefully* — a clean non-catchable fatal across all call paths (OP_CALL, eval/include, fibers), no silent NULL, no native-stack panic (§2) |

### Test infrastructure (`tests/phpt.php`)
| Sections supported | Sections **not** implemented |
|---|---|
| `--TEST--`, `--DESCRIPTION--`, `--CREDITS--`, `--SKIPIF--`, `--FILE--`, `--EXPECT--`, `--EXPECTF--`, `--CLEAN--` | `--POST--`, `--POST_RAW--`, `--GET--`, `--COOKIE--`, `--STDIN--`, `--INI--`, `--ARGS--`, `--ENV--`, `--EXPECTREGEX--` (`phpt.php:11-13`) |

No test in the current corpus uses an unimplemented section (0 `# TODO` in the last run),
so these are not blocking today. The runner now executes every test in its own child
process in both of its modes — see §3.7 for the remaining error-fidelity work.

**Test tiers** (`make`): `test-smoke` (`tests/ph7/001-smoke`, in-process, fast) +
`test-integration` (`tests/ph7/002-integration`, child-process) run by default via
`make test`; `test-compat` runs both under the real `php` too. **`make test-stress`**
(`tests/ph7/003-stress`) is **opt-in** (not in `make test`) — resource/fault-injection
tests run in child-process mode under a per-allocation cap (`PHL_MAX_ALLOC`, §2). It is the
home for OOM and future slow/limit tests. **Blocked-on for a better mechanism:** migrating
the OOM tests to per-test `--INI-- memory_limit=…` needs the `--INI--` section (above) plus
the **php.ini subsystem** (§3.6); until then the global `PHL_MAX_ALLOC` env knob is used.

---

## 2. Recently completed

These were open in earlier revisions of this document and are now done — do not re-plan
them. All re-verified against HEAD `3c2344e` on 10 Jun 2026.

- **Typed + `final` class constants (Tier 1, PHP 8.3 / 8.1).** Two related gaps shipped
  together — both live on the class-constant parse site (`GenStateCompileClassConstant`,
  `compile.c`) and the inheritance pass (`PH7_ClassInherit`, `oo.c`), and both reuse existing
  machinery (`ph7_class_attr` already carries `nType`/`sClass`/`sTypeName`/`aUnionAlts`; the
  `PH7_CLASS_ATTR_TYPED`/`FINAL` flags already exist — **no new bit or struct**).
  (1) **Typed constants** `const int X = 1`: a new `GenStateClassConstHasType` two-token
  lookahead decides whether a type precedes the name (a name-like token followed by `=`/`;`/`,`
  is the untyped form; anything else — `int X`, `?int X`, `A|B X`, `\Ns\Foo X` — is a type), so
  the common untyped path and multi-decl `const A = 1, B = 2` are untouched. The type is parsed
  once with the existing `GenStateParseUnionTypeDecl`, validated against the property
  pseudo-type blocklist (`GenStateValidateConstantType` → `callable`/`void`/`never` rejected with
  the PHP-exact `Class constant C::X cannot have type T`), copied onto the attr, and shared
  across a multi-decl chain. Enforcement is **strict** (unlike weak typed properties): a new
  `VmEnforceConstantType` (`vm.c`), run right after the initializer materializes in
  `VmMountUserClassAttrs`, accepts an exact type match plus the single int→float widening and
  nothing else (so `const float X = 1` is accepted but `const int X = "5"` / `= 1.0` are not),
  reusing `VmCoerceToUnion`(strict)/`VmCheckPseudoType`/`PH7_VmInstanceOf`. A mismatch is the
  PHP-exact non-catchable definition-time fatal `Cannot use T as value for class constant
  C::X of type U`. Because class mounting runs inside `ph7_compile_file` (before the host
  installs the VM output consumer), the fatal is routed through the **code-generator's** error
  consumer — which required keeping that consumer wired across `PH7_VmMakeReady`'s
  `PH7_ResetCodeGenerator` (it was nulled) and classifying a mount-time codegen error as
  `PH7_COMPILE_ERR` in `ProcessScript` (`api.c`). (2) **`final` constants** `final const Y = 2`:
  the class-body `final` branch now routes a following `const` (in either modifier order —
  `public final const`, `final public const`) to the constant compiler with
  `PH7_CLASS_ATTR_FINAL`; `PH7_ClassInherit` rejects a child redeclaration of a base final
  constant with `C::X cannot override final constant P::X`, naming the original declaring class
  (`pDeclClass`) so a multi-level chain matches PHP. Byte-for-byte parity with PHP 8.5.7;
  `make test` (2140 smoke + 686 integration), `test-compat` and `test-stress` all green.
  Regression: `oo/class_const_{typed,typed_union,typed_interface,final,untyped_unchanged}.phpt`
  (smoke), `oo/class_const_{typed_mismatch,typed_disallowed,final_override}.phpt` (integration).
  *Known cosmetic gap (§3.7 class):* a `self`-typed constant's TypeError renders `self` where
  PHP expands it to the class name. *Known lenient divergence (float-identity engine gap):* a
  whole-valued real is flagged `REAL|INT` and PHL's `/` always yields a real, so a `: int`
  constant accepts both `const int X = 1.0` (PHP rejects) and `const int X = 4/2` (PHP accepts as
  `int(2)`) — PHL cannot distinguish them by flag, so it accepts both rather than rejecting the
  valid `4/2`; a fractional real (`1.5`) is still correctly rejected. Tightening needs the
  float-identity/division model (out of scope). *Pre-existing, out of scope (surfaced while
  testing):* a constant whose initializer references another constant of the same class via
  `self::` resolves to 0 when the referenced constant is mounted later (hash-iteration order, not
  declaration order) — reproduces independent of this change.
- **`return` from inside a `catch`/`finally` now returns from the enclosing function (Tier 0
  correctness hazard).** Previously the value was silently dropped and control *fell through* to
  the code after the try/catch (`return` from a `try` body already worked — it is inline in the
  function bytecode). Root cause: catch/finally bodies are *separate* bytecode streams run via
  `VmLocalExec(…,pResult=0)` (`VmThrowException`, `OP_POP_EXCEPTION`), which discarded the
  return value with no path to signal the enclosing function to return; a catch/finally terminal
  `OP_DONE` was also indistinguishable from a bare `return;`. Fix (modeled on the
  `bHaltRequested`/`pPendingException` cross-`VmLocalExec` signal precedent):
  (1) the compiler marks an explicit `return` with `OP_DONE` `iP2=1` (`PH7_CompileReturn`),
  terminal catch/finally DONEs keep `iP2=0`; (2) a new `bReturnPropagates` arg on `VmLocalExec`
  (TRUE only for the catch/finally execs; FALSE for match/switch arms, default values, static
  init, etc.) threads into `VmByteCodeExec`; (3) when an `iP2` DONE runs in such a mini-program,
  the value is deferred to new VM fields `bReturnRequested`/`sCatchReturn` (`ph7int.h`) instead
  of the local `pResult`; (4) the enclosing function's `OP_POP_EXCEPTION` / `OP_DONE`
  materialize `sCatchReturn` into the real result and `goto Done`, after a shared
  **`VmDrainFinally`** helper runs any outer finally blocks (so `finally` overrides a catch
  return and the `try{ inner-catch returns }finally{…}` ordering is preserved). PHP edge
  semantics handled: a `throw` in `finally` discards a pending return (cleared at the top of
  `VmThrowException`); a `return` in `finally` swallows an in-flight exception (guarded
  deferred-rethrow). Frame safety: a return that unwinds across nested try/catch tears down any
  try frames whose `OP_POP_EXCEPTION` it bypassed, **bounded by the exec's entry frame**
  (`pEntryFrame`) so a tangled exception-in-finally chain can't over-leave or hang. Generators
  work unchanged (return surfaces via `Generator::getReturn()`). Byte-for-byte parity with PHP
  8.5.7; `make test` (2135 smoke + 683 integration) and `test-compat` green. Regression:
  `lang/return_in_catch_{basic,types,outer_finally,runs_finally}.phpt`,
  `lang/return_in_finally_{basic,swallows_exception}.phpt`, `lang/bare_return_in_catch.phpt`,
  `lang/finally_overrides_catch_return.phpt`, `lang/exception_in_finally_discards_return.phpt`,
  `lang/nested_try_catch_return.phpt`, `lang/return_in_try_inside_catch.phpt`,
  `lang/catch_no_return_unchanged.phpt`, `lang/return_in_try_with_finally_unchanged.phpt`.
  *Known limitation (not a regression — the unmodified engine drops these returns too, and the
  case is corruption-free):* the pending-return signal is a single VM-global, so it is clobbered
  when the `finally` that runs because of the return itself performs a **nested** throw/catch —
  e.g. `catch{return X} finally{ helper(); }` where `helper()` has its own try/catch, or an
  inline `try/catch` inside that `finally`. In those cases `X` is dropped (falls through, as
  before this fix). A correct fix needs per-exec signal scoping (save/restore the
  `bReturnRequested`/`sCatchReturn` pair across nested `VmByteCodeExec` boundaries); deferred as
  its own task because narrow variants regress the exception-in-finally cases above.
  *Pre-existing, out of scope (discovered while testing):* `yield` inside a `catch` body is not
  detected as making the function a generator, and throwing inside a generator's `try` body
  hangs/segfaults non-deterministically — both reproduce on the unmodified engine.
- **`catch` blocks now share the enclosing variable scope (Tier 0 correctness hazard).** The
  §3.1 item was filed as "closure call inside a catch fails (operand stack misaligned)"; the
  real defect was far broader and the diagnosis wrong. A matched catch ran its (separately
  compiled) body via `VmLocalExec` inside a frame `VmEnterFrame`'d and flagged **only**
  `VM_FRAME_CATCH`. Variable resolution (`VmExtractMemObj` → `VmSkipExceptionFrames`) only
  treats **`VM_FRAME_EXCEPTION`** frames as transparent, so the opaque catch frame gave the
  body a fresh empty scope: *no* enclosing local was visible (a closure in `$fn` resolved to
  null → the "Invalid function name" symptom), `$this` was unset inside a method's catch, and
  `$e`/any variable written in the catch was discarded on frame exit. The *try* body never had
  this bug because `OP_LOAD_EXCEPTION` (`vm.c`) wraps it in a deliberately transparent
  `VM_FRAME_EXCEPTION` frame; `finally` was unaffected (no opaque frame). Fix (one site,
  `VmThrowException`, `vm.c`): flag the catch frame `VM_FRAME_CATCH | VM_FRAME_EXCEPTION`
  *before* binding `$e`, so it is transparent — `$e`, enclosing locals, `$this` (itself a
  normal `this` variable in the method's real frame) and var writes all resolve against the
  real enclosing frame, and `$e` now persists after the catch (all PHP-exact). Safe because
  every other `VM_FRAME_EXCEPTION` code path is additionally guarded by `iExceptionJump > 0`
  (the catch frame's is 0); only the three transparency behaviours (var lookup, `global`
  uplink, skip-local-release-on-leave) change, all intended. Byte-for-byte parity with PHP
  8.5.7; full `make test` (2121 smoke + 683 integration) and `test-compat` green. Regression:
  `lang/catch_scope_{enclosing_var,closure_call,this,write_persists}.phpt`,
  `lang/catch_var_persists.phpt`. *Newly discovered while verifying (pre-existing, distinct,
  filed in §3.1):* `return` from inside a `catch`/`finally` body is silently dropped.
- **Traversable iteration: `iterator_to_array`/`iterator_count`/`iterator_apply` + Traversable
  spread (Tier 1).** Two roadmap items sharing one missing core. `foreach` drove the Iterator
  protocol inline but the logic was welded to the bytecode foreach-step state, so it couldn't be
  reused — and (a latent gap) it never propagated an exception thrown by an iterator method.
  Added a standalone **`PH7_VmIteratorWalk(pVm, pObj, xStep, pUserData)`** (`vm.c`, decl in
  `ph7int.h`): resolves `Iterator`/`IteratorAggregate`(`getIterator()`)/`Generator`, drives
  `rewind`/`valid`/`current`/`key`/`next`, invokes a per-element step, and **propagates
  `PH7_EXCEPTION`/`PH7_ABORT`** from any method or step (checks the `PH7_VmCallClassMethod` return
  the foreach loop discards). Consumers route through it:
  - `iterator_to_array(Traversable|array, bool $preserve_keys=true)`, `iterator_count(...)`,
    `iterator_apply(Traversable, callable, array $args=[])` — new builtins in `hashmap.c`
    (`aHashmapFunc[]`). `iterator_apply` re-resolves its `$args` each iteration (the iterator's own
    methods run user code between calls and may realloc `aMemObj`), stops on a falsy return, and
    propagates a throwing callback.
  - **Array-literal spread `[...$it]`** (LOAD_MAP) and **call-arg spread `f(...$it)`** (OP_SPREAD):
    walk a Traversable into the merge / a materialized temp array (deep-copied via
    `PH7_MemObjStore` so the temp frees immediately). Non-Traversable objects still raise the
    catchable `Only arrays and Traversables can be unpacked` Error.
  Byte-for-byte parity with PHP 8.5.7; `make test` (2116 smoke + 683 integration), `test-compat`,
  `test-stress` all green. Regression: `function/iterator_{to_array,count,apply}/*.phpt`,
  `function/iterator_to_array/iterator_to_array_throws.phpt`, `lang/spread_traversable.phpt`.
  *Pre-existing limitations discovered (out of scope — they affect arrays too, not just
  Traversables):* call-argument spread of a non-variable expression result — `f(...g())`,
  `f(...[1,2,3])`, `f(...arr())` — passes the operand whole instead of expanding; only
  `f(...$var)` / `f(...new X)` expand. And a same-frame `try/catch` directly around a
  call-arg spread whose iterator throws mid-walk propagates to an outer handler (the iterator
  builtins themselves are fully catchable in-frame). *Follow-up (separate, pre-existing):* the
  `foreach` opcode discards the return code of its iterator-method calls, so an exception thrown
  by `rewind`/`valid`/`current`/`key`/`next` during a `foreach` is swallowed — whereas
  `PH7_VmIteratorWalk` propagates it. Aligning `foreach` (propagate the same way) is a worthwhile
  follow-up but touches its resumable state machine, so it's out of scope here.
- **Type-declaration trio: `true`/`false` return enforcement, standalone `null`, `iterable`
  property (Tier 1).** Three related PHP-8.2/7.4 type-system gaps, two of them live bugs.
  (1) **`true`/`false` return enforcement was broken** — `function f(): true { return true; }`
  threw "must be of type true, bool returned" because the parser stores `true`/`false` as
  class-name atoms (`SXU32_HIGH`) and `VmEnforceReturnType` routed them through the
  class-instanceof branch. Fixed with a value-checking short-circuit (mirrors the `mixed`
  precedent): `:true` accepts a true bool, `:false` a false bool, else a PHP-exact TypeError
  (`VmValueGivenName` renders the literal). (2) **standalone `null`** — the compiler rejected
  null-only types ("Null can not be used as a standalone type"); bare `null` now parses
  (represented as `MEMOBJ_NULL`) and is enforced null-only on returns and properties; `?null`
  is still rejected (PHP does too). (3) **`iterable` property type** — was rejected
  (`GenStateIsDisallowedPropertyAtom`); now accepted and enforced as `array|Traversable` via an
  explicit branch in `VmEnforcePropertyTypeOnStore` (reusing `PH7_VmInstanceOf` +
  `Traversable`), which is required because the generic class branch would resolve no "iterable"
  class and wrongly accept any object / reject arrays. Files: `src/ph7/vm.c`
  (`VmEnforceReturnType`, `VmEnforcePropertyTypeOnStore`), `src/ph7/compile.c`
  (`GenStateParseUnionTypeDecl`, `GenStateIsDisallowedPropertyAtom`). Byte-for-byte parity with
  PHP 8.5.7; full suite green under phl and php. Regression:
  `lang/return_type_{true_false,null,literal_reject}.phpt`, `oo/typed_property_iterable.phpt`.
- **Pseudo-type enforcement completed across all sites (follow-up to the trio).** The trio
  above only enforced the pseudo-types `true`/`false`/`iterable`/`mixed` at *some* sites,
  leaving real gaps: `iterable` as a return/param type accepted any object, `true`/`false`
  params were accept-only (and a `mixed` property was still rejected; `true`/`false`/`iterable`
  in a union or `?true` mis-resolved as a class). Root cause was the altitude smell flagged in
  review — these pseudo-types are stored as `SXU32_HIGH` class-name atoms, so every enforcement
  site had to string-match them and only some did. Fixed by centralising the check in one
  helper, **`VmCheckPseudoType`** (`vm.c`): returns match / no-match / not-a-pseudo-type for a
  value against a name atom (`mixed`=any, `true`/`false`=the matching bool, `iterable`=
  array|Traversable via `pVm->pTraversableClass`). It is now called from all four enforcement
  sites — `VmEnforceReturnType`, `VmEnforcePropertyTypeOnStore`, the two parameter-binding
  paths, and `VmCoerceToUnion` — replacing the scattered inline `mixed`/`true`/`false` branches.
  `mixed` removed from `GenStateIsDisallowedPropertyAtom` (now accepts any value incl. null).
  Byte-for-byte parity with PHP 8.5.7; full suite + compat + stress green. Regression:
  `lang/return_type_iterable.phpt`, `lang/param_literal_pseudo_types.phpt`,
  `lang/union_literal_pseudo_types.phpt`, `oo/typed_property_mixed.phpt`.
  *Remaining (cosmetic, §3.7 error-fidelity audit — tracked there, not here):* an `iterable`
  TypeError still shows `iterable` where PHP renders the expanded `Traversable|array`.
- **Recursion-depth limit is now a clean, comprehensive bound (enables ESP32 B2).** Three
  problems: (1) hitting the cap in `PH7_OP_CALL` (`vm.c`) ran `VmErrorFormat(… "PH7 will set a
  NULL return value")`, popped args, set NULL and **continued** — not catchable and a silent
  wrong answer; (2) `eval()`/`include`/`require` (via `VmEvalChunk → VmLocalExec`) and (3)
  fibers/generators (`VmStartCtx`/`VmResumeCtx`) **bypassed the counter entirely**, so recursive
  include/eval or nested fibers could overflow the native stack and reboot a small-stack
  embedder. Fix: a shared `VmRecursionFatal()` (mirrors `PH7_VmMemoryError`) raises a
  **non-catchable fatal** — matching PHP 8.3's "Maximum call stack size reached" — sets exit
  status 255, requests a clean halt (unwinds via the abort path, still runs
  `register_shutdown_function`), and is idempotent so a recursing error handler can't re-enter.
  It is now called from **all three** paths: the OP_CALL cap check, `VmEvalChunk` (with a
  pre-check + `nRecursionDepth++/--` scoped to the eval/include caller, not the shared
  `VmLocalExec`), and the fiber/generator start/resume entry. *Catchable Error was prototyped
  and rejected:* PH7 runs catch bodies and renders uncaught exceptions **inline at the
  throw-site depth**, which is already over the cap, so `getMessage()`/`__toString()`/the catch
  body re-trip the limit and recurse forever. Defaults (32 host / 16 elsewhere) and
  `PH7_VM_CONFIG_RECURSION_DEPTH` (clamp `>2 && <1024`) are unchanged — the ESP32 port sets the
  cap per-stack. *Known nuance:* the `__toString`→string-cast path reports the fatal and exits
  255 but its conversion fallback still continues (pre-existing, unchanged here). Regression:
  `tests/ph7/002-integration/lang/recursion_limit_{fatal,eval}.phpt`; migrated
  `oo/magic/tostring_recurse.phpt` to the new message.
- **`**` / `**=` exponentiation** (5.6) — `EXPR_OP_POW` / `PH7_OP_POW` in `parse.c`/`vm.c`.
- **Nested short-array literals with `=>`** (5.4) — fixed bracket-depth in `GenStateCompileArrayBody`.
- **Array unpack (spread) with string keys** (8.1) — `MEMOBJ_AUX_SPREAD` flag +
  `PH7_OP_FLAG_SPREAD` + `PH7_HashmapMerge` (reuses `array_merge` semantics). Non-array
  spread throws PHP-exact `\Error`/`\TypeError`. (Traversable spread now ✅ via
  `PH7_VmIteratorWalk` — see the dedicated §2 entry above.)
- **Perl-style string increment** (`"a"++`→`"b"`) — `PH7_MemObjStringIncrement` (`memobj.c`).
- **`__invoke`** — `$obj()`, `call_user_func[_array]`, `usort`, `array_map`,
  `preg_replace_callback`; missing `__invoke` → catchable `Error`.
- **`str_contains` / `str_starts_with` / `str_ends_with`** (8.0).
- **`random_int` / `random_bytes`** (7.0) — OS CSPRNG.
- **SPL marker interfaces** (8.x) — ArrayAccess, Countable, Stringable, Traversable,
  UnitEnum, BackedEnum declared and dispatched (`$obj[$k]`, `count($obj)`, auto-`Stringable`
  for `__toString`, `instanceof Traversable` chain walk). `Iterator`/`IteratorAggregate`
  now `extends Traversable`.
- **Float type identity (no more int-collapse)** — integer-valued reals (`1.0`, `1.0+1.0`,
  `$f++`/`$f--`) were stored dual-flagged `REAL|INT` and reported as int by
  `is_int`/`gettype`/`var_dump`. The `REAL` flag is now authoritative (`ph7_value_is_int`,
  `PH7_MemObjTypeDump`), making the cached-int benign engine-wide; `var_dump` labels reals
  `float(` (PHP) while `gettype` keeps `"double"`. Byte-for-byte parity with PHP 8.5.
- **`--` on non-numeric strings is a no-op** (`"abc"--` stays `"abc"`; PHP has no string
  decrement) — `PH7_OP_DECR` rewritten to mirror the hardened `PH7_OP_INCR`
  (`VmStringWantsPerlIncr` guard), also fixing a `pTos`/`pObj` aliasing bug. `null--` no-op
  preserved. Regression: `tests/ph7/001-smoke/lang/{float_type_identity,
  increment_decrement_semantics}.phpt`.
- **Tier 0 correctness bugs (all three)** — shipped together:
  - **`HashmapMerge`/spread/`array_merge` by-ref drop** — `HashmapDuplicateNode`
    (`hashmap.c`) value-copied every entry, flattening references (`[&$x]`). It now
    detects `HASHMAP_NODE_FOREIGN_OBJ` source nodes and re-inserts by reference via
    `PH7_HashmapInsertByRef`, so spread (`[...$a]`), `array_merge`, `array_replace` and
    array copies keep references live. Regression: `short_array_spread_reference.phpt`,
    `array_merge_preserves_references.phpt`.
  - **Multi-array `array_map($cb,$a,$b,…)`** — `ph7_hashmap_map` now walks every array in
    parallel to the longest one (padding short arrays with `null`), re-indexes the result,
    and zips on a `null` callback; the single-array path keeps keys. Regression:
    `array_map_multi_arrays/_pad_null/_reindex.phpt`, `array_map_null_zip.phpt`.
  - **`__invoke` / constructor / callback exception unwind** — `VmCallClassMethodWithMap`
    and the plain-callable `PH7_VmCallUserFunction` branch discarded `VmByteCodeExec`'s
    return code (`return PH7_OK`), so an exception inside `__invoke`, an array callable, a
    `NEW` constructor, or a `call_user_func`/array-builtin callback ran the `catch` but then
    continued past the failed call. The code is now propagated and each OP_CALL site
    (object `__invoke`, array-callable, OP_NEW) resumes after the frame's own try when the
    catch ran in-place, else `goto Exception` so it unwinds through intermediate frames.
    Builtins that took callbacks (`call_user_func[_array]`, `array_map`/`_filter`/`_reduce`/
    `_walk`/`_walk_recursive`, `usort`/`uasort`/`uksort`, `preg_replace_callback`) now
    return `PH7_EXCEPTION` (sort comparators via a new `pVm->iCmpCallbackExc` flag, short-
    circuiting further comparisons). `PH7_EXCEPTION` was promoted from a `vm.c`-local macro
    to `ph7int.h`. Regression: `oo/magic/invoke_exception_{unwinds,nested,call_user_func}.phpt`,
    `oo/exception_in_{array_callable,constructor_strict_types}.phpt`,
    `function/array_map/array_map_callback_exception.phpt`, and the corrected
    `oo/throw_in_constructor.phpt`.
- **`array_udiff` / `array_uintersect` / `array_diff_uassoc` callback-exception swallow** —
  these user-comparison builtins discarded a callback exception (the same
  comparator-returns-int pattern fixed earlier for the `usort` family): the `catch` ran but
  execution continued past the call. `HashmapFindValueByCallback` (shared by `array_udiff`/
  `array_uintersect`) now sets `pVm->iCmpCallbackExc` on `PH7_EXCEPTION` and short-circuits
  further callback invocations; both builtins check the flag after their diff/intersect loop
  and return `PH7_EXCEPTION`. `array_diff_uassoc` (which invokes the key comparator inline)
  releases its temporaries and propagates directly. Byte-for-byte parity with PHP 8.x on
  both the result and the exception paths. Regression:
  `function/array_udiff/array_udiff_callback_exception.phpt`,
  `function/array_uintersect/array_uintersect_callback_exception.phpt`,
  `function/array_diff_uassoc/array_diff_uassoc_callback_exception.phpt`.
- **`#[ … ]` attribute groups no longer swallow the rest of the line** — `#[` was lexed
  as a `#` line comment, so `#[Attr] function f(){}` silently discarded the function
  (silent code loss on any single-line attribute). The lexer (`lex.c`, `TokenizePHP`) now
  skips the balanced `#[ … ]` group as inert trivia: bracket-depth counted; brackets
  inside string literals (escape-aware) and inline/block comments ignored; `nLine` kept
  accurate across multi-line groups; unterminated groups consume to EOF silently
  (consistent with unterminated `/* */`). Attributes are still not stored — parse+store
  remains a §3.2 item. Known limitation: heredoc literals inside attribute arguments.
  Regression: `lang/attribute_{same_line_code,class_members,args_strings,nested_brackets,
  multiline_line_count}.phpt`, `lang/hash_comment_unchanged.phpt`,
  `lang/string_hash_bracket_literal.phpt` — all verified byte-identical under PHP 8.5.
- **`exit`/`die` now unwinds the VM (PHP semantics) — three engine fidelity bugs.**
  (1) `exit`/`die` *inside an included file* called `exit(C process)` directly
  (`vm.c` OP_HALT + `vm_builtin_exit`: `if(SySetUsed(&aFiles)) exit(...)`), hard-killing
  the process and **skipping shutdown callbacks**. (2) `exit`/`die` *inside `eval()`*
  didn't halt at all — `VmEvalChunk` swallowed the abort, so execution continued past the
  `eval()`. (3) `phl -r 'exit(3)'` returned process status **0** — `phl.c` called
  `ph7_vm_exec(pVm, 0)`, discarding the status the API exposes. Fixes: a `bHaltRequested`
  flag on the VM (`ph7int.h`); OP_HALT and `vm_builtin_exit` set it and `goto Abort` /
  `return PH7_ABORT` unconditionally instead of `exit()`; the include/require/eval
  builtins cascade `PH7_ABORT` while the flag is set, so the halt unwinds to the top where
  `VmInvokeShutdownCallbacks` runs normally; `exit()` *inside* a shutdown callback re-sets
  the flag and short-circuits the remaining callbacks (PHP behavior); `phl.c` propagates
  the real status. Byte-for-byte parity with PHP 8.5. Regression:
  `function/exit/exit_in_include_runs_shutdown.phpt`, `exit_in_eval_halts.phpt`,
  `exit_in_function_in_include.phpt` (status propagation verified by command, §7).
- **Test runner: silent truncation made impossible (in-process default kept).** The
  runner still runs the smoke corpus **in-process** by default (fast — ~4s; the corpus is
  curated to never `exit`/`die` or pollute the interpreter). The old failure mode — a test
  calling `die()` killed the harness at test 2361 with exit 0 and no summary — is gone:
  the engine fix above lets the harness's top-level `register_shutdown_function` run even
  after a test dies, and a new **abort guard** in `tests/phpt.php` (flushes the aborted
  test's open `ob_start()` buffer, then prints TAP `Bail out!` and `exit(1)`) turns any
  interpreter-killing test into a loud failure. Byte-identical under phl and real php.
  Tests that legitimately exit/die live in `002-integration` and run with
  `--target-executable` (child process). **`PHP_BINARY`** constant added (`phl.c`,
  `realpath` / `GetModuleFileNameA`; real PHP already defines it) — a framework-compat win
  and verified by `constants/php_binary.phpt`. Harness self-diagnostics:
  `tests/phptrunner/007-handler_format_test.diag` (in-process normalized format),
  `008-exit_test.diag` (die() → `Bail out!`, intentionally last). Harness artifacts
  (`*.phpt.file` etc.) now gitignored.
- **Multiple array *literals* as call args no longer collapse** — `array_merge([1],[2])`
  returned `[2]` and `f([1],[2],[3])` reached the callee as one arg, because the
  argument splitter `ExprProcessFuncArguments` (`parse.c`) incremented its bracket-nesting
  counter on a short-array node's `[` but that node had already consumed its own `]`
  (no balancing close-bracket node), so `iNest` never returned to 0 and every later comma
  was hidden. Self-contained `PH7_CompileShortArray`/`PH7_CompileShortList` nodes are now
  treated as terms, not opening brackets. `array()` and variables masked the bug, which
  affected *any* call, not just array builtins. Regression:
  `function/array_merge/array_merge_short_array_literal_args.phpt`,
  `parse/call_arg_short_array_literals.phpt`.

---

- **PHP version surface + SAPI name (Tier 1).** `PHP_VERSION`, `PHP_MAJOR_VERSION`,
  `PHP_MINOR_VERSION`, `PHP_RELEASE_VERSION`, `PHP_EXTRA_VERSION`, `PHP_VERSION_ID`,
  `phpversion()` and `php_sapi_name()` were all undefined — framework version gates
  (`version_compare(phpversion(), …)`, `PHP_VERSION_ID < 80000`) crashed or branched wrong.
  A single source of truth — `PHP_COMPAT_*` macros in `ph7.h` (currently **8.5.0** /
  `80500`, matching the parity target) — feeds six new constant callbacks (`constant.c`,
  registered in `aBuiltIn[]`) and the `phpversion()` builtin (`vm.c`). `phpversion()` is
  no-arg → version string, extension-arg → NULL (no extension registry). `php_sapi_name()`
  reads `pVm->bHttpContext` → `"cli"` on the CLI, `"cli-server"` under `-S`. Regression:
  `constants/php_version.phpt` (asserts the
  `ID = major*10000+minor*100+release` identity, so it validates structurally under real
  PHP too), `function/phpversion/phpversion.phpt`, `function/php_sapi_name/php_sapi_name.phpt`
  — all pass byte-for-byte under phl and real PHP 8.5.7.
- **Array helpers `array_column` / `array_is_list` / `array_find` / `array_find_key` /
  `array_any` / `array_all` (Tier 1).** Six new builtins in `hashmap.c`
  (`aHashmapFunc[]`). `array_is_list` checks for consecutive 0-based int keys (empty →
  true). `array_column` supports a null column-key (whole row), an optional index-key,
  skips rows lacking the column, appends rows lacking the index-key, and reads both array
  rows (`PH7_HashmapLookup`) and object rows with declared properties
  (`ph7_object_fetch_attr`, via a shared `HashmapColumnFetch`). The find-family share
  `HashmapCallbackSearch`, which invokes `$callback($value, $key)` (PHP 8.4 order) and
  propagates a callback exception as `PH7_EXCEPTION` (same discipline as `array_filter`):
  `array_find` → first matching value or null, `array_find_key` → first matching key,
  `array_any` → true if any match (false on empty), `array_all` → true if all match (true
  on empty, by hunting for the first non-match). Regression:
  `function/array_{column,is_list,find,find_key,any,all}/*.phpt`, all byte-for-byte under
  phl and real PHP 8.5.7. Two pre-existing engine bugs surfaced while testing (recorded,
  out of scope here): `json_encode` drops keys for non-list arrays, and the `(object)`
  array-cast yields inaccessible dynamic properties — the tests use an explicit foreach
  formatter and declared-property objects to avoid both.
- **`json_encode` now emits a JSON object for non-list arrays (correctness hazard, §3.1).**
  The encoder always emitted a JSON *array*, so `json_encode(["x"=>1])` → `[1]` and
  `json_encode([1=>"a",2=>"b"])` → `["a","b"]` (silent wrong answer, high frequency in API
  payloads). PHP's rule — serialize as an array iff `array_is_list()` (consecutive 0-based
  int keys; empty → `[]`), else as an object with stringified keys — is now applied. The
  list-detection logic that was inline in `array_is_list` (`hashmap.c`) is extracted into a
  shared `PH7_HashmapIsList(ph7_hashmap*)` (declared in `ph7int.h`); `ph7_hashmap_is_list`
  and the encoder both call it. `VmJsonEncode` (`vm_json.c`) computes the per-array
  object-vs-array choice (`JSON_FORCE_OBJECT` still forces object) and saves/restores a new
  `isObject` field on the encoder state around the `ph7_array_walk`, so the key-emission
  callback `VmJsonArrayEncode` keys correctly across object→list→object nesting. Byte-for-
  byte parity with PHP 8.5.7. Regression: `function/json_encode/json_encode_{assoc_object,
  list_array,nested_mixed}.phpt`. (The separate `(object)` array-cast dynamic-property gap
  remains open — §3.1.)
- **`json_last_error_msg()` (5.5) + `json_validate()` (8.3) (Tier 1).** Both were
  undefined. `json_last_error_msg` (`vm_json.c`) is the string sibling of the existing
  `json_last_error` — a `switch` over `pVm->json_rc` returning the PHP-exact messages
  (`No error`, `Syntax error`, …). `json_validate` checks a string's JSON validity without
  materializing a value: the tokenize+decode core of `json_decode` was extracted into a
  shared `VmJsonDecodeInput()` (returns the resulting `json_rc`), which both `json_decode`
  and `json_validate` now call — `json_validate` discards the decoded value and returns
  `json_rc == JSON_ERROR_NONE` as a bool. The empty string is reported invalid
  (`JSON_ERROR_SYNTAX`) — unlike `json_decode("")`, which returns NULL without setting the
  code — and decode runs in associative mode so the engine's "objects are returned as an
  array" warning is suppressed. `$flags` is accepted and ignored (no decode flag is
  implemented). Registered in `aVmFunc[]` (`vm.c`). Byte-for-byte parity with PHP 8.5 (the
  one known `bool(TRUE)`-casing var_dump gap, §3.7, is avoided). Regression:
  `function/json_last_error_msg/json_last_error_msg.phpt`,
  `function/json_validate/json_validate.phpt`.
- **`JsonSerializable` (5.4) + two return-type engine fixes it surfaced (Tier 1).** The
  interface was undeclared and `json_encode` blindly serialized an object's public
  properties, ignoring `jsonSerialize()` — silently wrong JSON for any DTO/value object that
  controls its own shape. Added as an embedded-PHP interface (`vm.c:1079`,
  `jsonSerialize()`), cached on the VM (`pJsonSerializableClass`, `ph7int.h` / `vm.c` init,
  mirroring `pStringableClass`). `VmJsonEncode`'s object branch (`vm_json.c`) now checks
  `PH7_VmInstanceOf(…, pJsonSerializableClass)` and, if implemented, calls `jsonSerialize()`
  (`PH7_ClassExtractMethod`+`PH7_VmCallClassMethod`) and re-encodes the returned value
  (scalar/array/object); otherwise it falls back to the property walk. A throw inside
  `jsonSerialize()` propagates: `VmJsonEncode` returns `PH7_EXCEPTION` and a new `exc` flag on
  `json_private_data` short-circuits the array/object walk callbacks (same discipline as
  `iCmpCallbackExc`), so `vm_builtin_json_encode` returns `PH7_EXCEPTION`. Two **pre-existing
  engine bugs** were found and fixed to make idiomatic `jsonSerialize(): mixed` usable:
  - **`mixed` return type rejected all non-object values** — `mixed` parses as a class-name
    atom (`SXU32_HIGH`, name "mixed") since it is not a scalar keyword, so the class-return
    branch demanded an object and threw "Return value must be of type mixed, X returned" for
    arrays/strings/ints/null. `VmEnforceReturnType` (`vm.c`) now short-circuits `mixed` to
    accept any explicitly returned value (incl. null) — after the no-value check, so a
    fell-off-end `: mixed` still errors like PHP. (`mixed` already worked as a param type.)
  - **Typed function that throws raised a spurious second `TypeError`** — the compiler routes
    an uncaught `throw` to the function's terminal `OP_DONE`, which then ran return-type
    enforcement and threw "Return value must be of type X, null returned" *over* the real
    exception (reproduced with a plain `function f(): int { throw …; }`, not just
    `jsonSerialize`). `OP_DONE` now skips enforcement when the frame is unwinding
    (`VM_FRAME_THROW`). Byte-for-byte parity with PHP 8.5.7. Regression:
    `function/json_encode/json_encode_jsonserializable{,_throws}.phpt`,
    `lang/mixed_return_type.phpt`.
- **`preg_match`/`preg_match_all` `$matches` now populate undefined variables — engine-wide
  by-ref builtin auto-vivify (Tier 0 correctness hazard).** The capture-population logic
  (`PcrePopulateMatches`/`PcreStoreByRef`, `vm_pcre.c`) was already complete, but only wrote
  back when the caller pre-initialised the variable (`$m = null;`). A bare undefined
  `$matches` reached the builtin tagged `nIdx == SXU32_HIGH`, so the write-back was a silent
  no-op (the §7 probe returned `null`). Root cause was general: PH7 foreign/builtin functions
  carry no parameter signature, so the call compiler emitted *read-only* loads
  (`EXPR_FLAG_RDONLY_LOAD` → `OP_LOAD` `iP1=1`, no create) for *every* argument. Fix is
  compiler-side: a small by-ref builtin registry (`GenStateByRefBuiltinMask`, `compile.c` —
  `preg_match`/`preg_match_all` `$matches`, `preg_replace`/`preg_replace_callback` `&$count`)
  consulted at call-argument compile time; for a matched position the read-only flag is
  cleared so the variable auto-vivifies to a real memobj slot (`OP_LOAD` `iP1=0`,
  `bCreate=TRUE`, `vm.c`), giving the existing write-back a valid `nIdx`. The bare name is
  recovered via `GenStateCallBuiltinName` (handles unqualified `preg_match` and the absolute
  single-component `\preg_match`; rejects `Foo\preg_match` and method/static calls).
  Guarded off when spread/named args are present (compile-time position no longer maps to
  runtime `apArg[]`). PHP-exact "passing an undefined var by reference creates it" behavior;
  no-match sets `$matches = []`. *Known limitation:* subscript/property by-ref targets
  (`$a['k']`) still don't populate (needs `EXPR_FLAG_LOAD_IDX_STORE`, unchanged). Byte-for-
  byte parity with PHP 8.5.7. Regression:
  `function/preg_match/preg_match_undefined_{matches,no_match,named_groups,absolute_name}.phpt`,
  `function/preg_match_all/preg_match_all_undefined_matches.phpt`,
  `function/preg_replace/preg_replace_undefined_count.phpt`.
- **Silent allocation failure now surfaces a PHP-visible OOM fatal (Tier 0 correctness
  hazard).** A failed allocation inside a string/array builtin fabricated an empty/truncated
  value with a *success* status (`str_repeat('A',2_000_000)` → `""`, HTTP 200 on the ESP32
  profile). Root cause: the API layer discarded the allocator's `SXERR_MEM` —
  `ph7_value_string`/`ph7_value_string_format` (`api.c`) always returned `PH7_OK` — and
  builtins ignored even propagated codes (`str_repeat` broke its loop but returned `PH7_OK`;
  `StringReplace` *explicitly* swallowed `SXERR_MEM`; `OP_CAT`/`OP_CAT_STORE` dropped the
  append rc). Fixes: the keystone API functions now propagate the rc; a shared
  `PH7_VmMemoryError`/`PH7_ContextMemoryError` helper (`vm.c`) emits a **non-catchable**
  fatal (matching PHP's OOM), sets `iExitStatus=255`, sets `bHaltRequested` and returns
  `PH7_ABORT` — which unwinds via the existing abort path and still runs
  `register_shutdown_function` callbacks. Wired into `str_repeat`, `OP_CAT`/`OP_CAT_STORE`,
  `StringReplace`(+`str_replace`/`str_ireplace`/`strtr`), and the array-growth builtins
  `array_fill`/`array_pad`/`range`; two pre-existing `throw_error+return PH7_OK` sites
  (`str_getcsv`, `count_chars`) converted; and an **abort-path leak** fixed (the `OP_CALL`
  `PH7_ABORT` branch now releases the partial result `sRet`, mirroring the exception branch).
  A per-allocation cap (`SyMemBackend.nMaxRequest`, set via the new `PH7_CONFIG_MAX_ALLOC`
  engine verb / `PHL_MAX_ALLOC` env in `phl.c`) makes OOM deterministically testable. The
  default suites run uncapped (`nMaxRequest=0`), so behavior is unchanged unless an
  allocation genuinely fails. Verified: `make test`/`test-compat` green; new opt-in
  `make test-stress` (`tests/ph7/003-stress/memory/{str_repeat,array_fill,concat,
  shutdown_on}_oom.phpt`) asserts the fatal (not a wrong answer) and that shutdown runs.
  *Deferred (same helper):* broader audit of other allocation-ignoring sites (`implode`,
  `str_pad`, `sprintf`, `json_encode`, general hashmap-insert callers) — they degrade
  gracefully today, just don't yet raise.
- **SPL exception classes (Tier 1).** The 13 SPL exceptions (RuntimeException,
  LogicException, InvalidArgumentException, DomainException, LengthException,
  OutOfRangeException, OutOfBoundsException, OverflowException, UnderflowException,
  RangeException, UnexpectedValueException, BadFunctionCallException, BadMethodCallException)
  were undeclared — `class_exists("RuntimeException")` was false, so any framework/library
  code throwing or catching them died with "undefined class". Added as embedded PHP source
  in `PH7_BUILTIN_LIB` (`vm.c`, after `ErrorException`) following the existing
  `class X extends Error { }` template — no C. They use PHP's exact **two-root hierarchy**
  (LogicException/RuntimeException `extends Exception`; the rest extend those;
  BadMethodCallException `extends` BadFunctionCallException), so `catch`/`instanceof`/
  `get_parent_class` match PHP (e.g. `catch (LogicException)` catches
  InvalidArgumentException; OutOfBoundsException is a RuntimeException, not a LogicException).
  Each is an empty body inheriting Exception's constructor + `getMessage`/`getCode`/… .
  Declared-class count 14 → 27. Byte-for-byte parity with PHP 8.5.7. Regression:
  `lang/spl_exception_{class_exists,hierarchy,instanceof,catch_by_base}.phpt`.
- **VM-reset completeness — compile-once / execute-many (§3.8).** `ph7_vm_reset()`
  (`PH7_VmReset`, `vm.c`) previously cleared only the output blob, the `sExec` return value
  and the HTTP response headers, so re-executing one compiled VM bled global variables,
  superglobals, function/class statics, runtime closures, output buffers, the reference
  table and every allocated object into the next run — and the `-S` server therefore
  **recompiled the script on every request**. `PH7_VmReset` now restores the VM to its
  post-`PH7_VmMakeReady` state while preserving the compiled program and all *definitions*
  (bytecode, function/class tables, **user-defined constants**, the literal pool, operand
  stack, cached interface pointers, output-consumer config, IO streams). The mechanism: a
  watermark (`nSuperBaseline`, `ph7int.h`) is snapshotted in `PH7_VmMakeReady` just before
  the superglobals are created; reset unlinks the whole reference table (before releasing
  objects, so by-ref array nodes never dangle), releases the per-exec object pool
  `aMemObj[nSuperBaseline..]` (engine teardown only — user `__destruct` is suppressed via
  `bInReset` to stay crash-safe, matching PH7's prior no-global-destructor behaviour), frees
  runtime closures and resets all function/method static sentinels in one pass over
  `hFunction` (closures identified by `VM_FUNC_CLOSURE` — templates are never installed
  there), truncates the object pool back to the watermark, then re-runs the real init
  helpers — `VmEnterFrame`, `PH7_HashmapCreateSuper`, and a new `VmMountUserClassAttrs` (the
  static/const-attr half split out of `VmMountUserClass`, so methods are *not* re-installed).
  Definitions persist deliberately: `include_once` state (`aIncluded`) is kept so
  runtime-included definitions survive without recompiling or redeclare errors, and user
  constants are not trimmed (a re-run `define()` overwrites the value in place, freeing the
  old one — no leak). The `-S` dev server now compiles each script once and reset-reuses it
  per request via a small path-keyed VM cache with mtime invalidation (`server.c`, so edits
  are still picked up; `PHL_NO_REUSE=1` forces the legacy path). Verified: no state bleed
  across requests (globals, statics, class statics, closures, superglobals), definitions/
  constants persist across `include_once`, and flat memory (~16 KB drift / 2000 requests).
  **Unblocks** ESP32 B1. Regression:
  `tests/ph7/002-integration/server/vm_reuse_no_bleed.phpt`; full suite green under phl and
  php 8.5.7.
- **Amortized-O(1) `.=` string concatenation (§3.8).** `$s .= "..."` in a loop was O(n²)
  (host n=80k = 11.57 s; on-chip 64 KB = 79 s) — the biggest blocker to PHP-as-templating.
  The diagnosis in the prior §3.8 text was wrong: `SyBlob` growth is already super-geometric
  (`BlobPrepareGrow`, `sxmem.c`), and `str_repeat`/`implode`/echo-output are already linear.
  The real cost was three full O(n) buffer copies per `.=` in `OP_CAT_STORE` (`vm.c`): the
  loaded lvalue is a read-only alias of the variable's buffer, so the append copy-on-write
  dups it, then two `PH7_MemObjStore`/`SyBlobDup` calls copy the result to the slot and the
  stack. Fix: an in-place fast path in `OP_CAT_STORE` that appends straight into the lvalue's
  *owned* slot buffer (geometric → amortized O(1)), then aliases the result onto the stack —
  for plain variable, array-element and object-property lvalues. Guards fall back to the
  unchanged slow path when the slot is read-only/typed/constant or the RHS aliases the same
  slot (`$s .= $s`); typed properties keep slow-path type enforcement. OOM stays a clean
  fatal (grow precedes copy). Result: n=80k `.=` 11.57 s → 0.01 s (array/property forms too),
  byte-for-byte parity with PHP 8.5.7. Explicit `$s = $s . x` (separate `OP_CAT`+`OP_STORE`)
  remains O(n²) by design. **Unblocks** ESP32 templating (the §3.8 prerequisite). Regression:
  `tests/ph7/001-smoke/lang/concat_assign_inplace.phpt`,
  `tests/ph7/003-stress/memory/concat_assign_oom.phpt`.
- **`microtime(true)` / `gettimeofday` µs resolution + `PH7_CONFIG_CLOCK` embedder hook
  (§3.5).** Three defects in `builtin_date.c`: (1) the float branch of both `microtime(true)`
  and `gettimeofday(true)` returned `(double)tm_sec`, computing `tm_usec` and then discarding
  it — so `$t=microtime(true)` profiling was useless on *every* platform; (2) the
  non-`__UNIXES__` path faked sub-second time as `tt % SX_USEC_PER_SEC` (nonsense), leaving the
  ESP32 build (`-DOS_OTHER`) with no real clock; (3) `microtime()`'s *string* form printed raw
  integer microseconds (`"506671 1700000000"`) instead of PHP's fractional-seconds
  `"0.50667100 1700000000"`. Fixes: the float branch now returns `sec + usec/1e6`; the string
  form emits PHP-exact `0.dddddddd sec`; and a single shared `DateNow()` helper centralises
  "now", consulting an optional embedder clock first. The clock is installed via a new
  engine-level config verb **`PH7_CONFIG_CLOCK`** (`ph7.h`, two args: `ph7_clock xClock` +
  user data; stored on `ph7_conf`, reached from builtins via `pCtx->pVm->pEngine`; NULL =
  platform default; inherited by VMs — mirrors `PH7_CONFIG_MAX_ALLOC`). The hook fills epoch
  seconds + microseconds and returns `PH7_OK`. With no hook, `DateNow` uses `gettimeofday()` on
  Unix and a second-resolution `time()` (usec=0) elsewhere. **Unblocks** the ESP32 port: it can
  register `esp_timer` via `PH7_CONFIG_CLOCK` so standard `microtime(true)` works on-chip,
  retiring the bespoke `esp_us()` PHP shim (port wiring is the esp32s3-port follow-up).
  Byte-for-byte parity with PHP 8.5.7. Regression:
  `tests/ph7/001-smoke/function/microtime/microtime_subsecond.phpt`,
  `tests/ph7/001-smoke/function/gettimeofday/gettimeofday_subsecond.phpt`; the C-level hook is
  exercised by `examples/ph7_clock_hook.c`.

---

## 3. Remaining gaps (detail)

### 3.1 Correctness hazards (silent wrong answers — fix first)

All previously listed Tier 0 bugs are fixed (§2), including the two surfaced by the
10 Jun 2026 re-verification: ~~`#[Attr]` line-swallow~~ ✅ (§2) and ~~unsound default
test-runner mode~~ ✅ (§2). Known hazards still open, tracked here because they accept
invalid code or return silently wrong answers:

- ~~**`json_encode` drops keys for non-list arrays**~~ ✅ (§2) — now gated on
  `PH7_HashmapIsList` (the shared `array_is_list` logic), so non-list arrays emit a JSON
  object. *Still open:* `(object)` array-cast / stdClass dynamic properties are inaccessible
  (`$o=(object)["id"=>1]; $o->id` errors) — separate engine gap, lower frequency.
- ~~**Closure call inside a `catch` block fails**~~ ✅ (§2) — the prior "operand stack
  misaligned" diagnosis was wrong; the real defect was far larger: a `catch` block ran in an
  isolated frame with its own empty variable scope, so it saw *no* enclosing variable (the
  closure symptom was just `$fn` resolving to null → "Invalid function name"), nor `$this`,
  and `$e`/any var written in the catch was lost afterwards. Fixed by flagging the catch frame
  `VM_FRAME_EXCEPTION` (transparent) so it shares the enclosing scope — see §2.
- ~~**`return` from inside a `catch` or `finally` block is silently dropped**~~ ✅ (§2) — the
  value was discarded and control fell through to the code after the try/catch. Fixed with an
  explicit-return marker (`OP_DONE` `iP2`), a per-exec `bReturnPropagates` flag on the
  catch/finally `VmLocalExec`, and a `bReturnRequested`/`sCatchReturn` VM signal materialized at
  the enclosing function's `OP_POP_EXCEPTION`/`OP_DONE`, with bounded frame unwinding — see §2.
- **No method-override signature checks at all** — `class Q extends P { function f(): string }`
  over `f(): int`, and parameter-narrowing overrides, are silently accepted. (The previous
  revision claimed "overrides must match exactly" — wrong: nothing is checked, which is
  why covariant code happens to run.) Permissive, so valid code is not broken; the gap is
  that *invalid* code runs with no diagnostic. Fold into the covariance work (§3.3) — the
  real task is "implement override-compatibility checks with PHP 7.4 variance rules",
  not "relax an existing check". **M.**
- ~~**Silent allocation failure**~~ ✅ (§2) — allocation failure in string/array builtins
  fabricated an empty/truncated value with a success status; now surfaced as a PHP-visible
  non-catchable OOM fatal via the shared `PH7_VmMemoryError` helper. *Deferred (same
  pattern):* the broader audit of other allocation-ignoring sites (`implode`, `str_pad`,
  `sprintf`, `json_encode`/`vm_json.c`, and the general `ph7_array_add_*`→`PH7_HashmapInsert`
  callers across the engine) — they degrade gracefully (no crash), just don't yet raise.
- ~~**`preg_match` does not populate `$matches`**~~ ✅ (§2) — the population logic existed
  but only fired when the caller pre-initialised the variable; an *undefined* `$matches`
  reached the builtin tagged `nIdx == SXU32_HIGH` (read-only arg load) and the write-back
  was a silent no-op. Fixed engine-wide: the call compiler now auto-vivifies known by-ref
  builtin out-params (`GenStateByRefBuiltinMask`, `compile.c`). *Known limitation:* a
  subscript/property by-ref target (`preg_match($p,$s,$a['k'])`) still won't populate the
  element (needs `EXPR_FLAG_LOAD_IDX_STORE`, unchanged from before).

### 3.2 Language / parser

- **`yield from`** (7.0) — currently `Invalid array name`. Lex `from` after `yield`; opcode
  iterates source generator/iterable/array forwarding values+keys, forwards
  `send()`/`throw()`, captures return value; handle nesting. **M.**
- **Keyed list destructuring** (7.1) — `["k"=>$v]=$data` and `list("k"=>$v)=…` both fail
  with `Unexpected token '=>'`; extend list/short-list compiler to accept key expressions
  and emit key-based lookup; allow nesting and `foreach (… as ["k"=>$v])`. **M.**
- **First-class callable `f(...)`** (8.1) — detect lone `...` in an arg list, emit a Closure
  referencing the callable. Depends on **Closure class** (§3.4). **M.**
- **Intersection `A&B`** (8.1) + **DNF `(A&B)|C`** (8.2) — treat `&` as intersection in type
  context (not by-ref); add an intersection-alts container parallel to `aUnionAlts`
  (`GenStateParseUnionTypeDecl`, `compile.c:445`); parse parenthesised groups into a type
  tree; verify at call/return. **M (bundled).**
- ~~**`iterable` pseudo-type** (7.1)~~ ✅ (§2) — now accepted as a property type and enforced as
  `array|Traversable` (`VmEnforcePropertyTypeOnStore`). *Known cosmetic gap:* the TypeError text
  shows `iterable` where PHP expands it to `Traversable|array` (§3.7 error-fidelity class).
- ~~**`true`/`false` as types** (8.2)~~ ✅ (§2) — return-type enforcement now checks the boolean
  VALUE (`VmEnforceReturnType` short-circuit, mirroring `mixed`) instead of demanding an object.
  *(Out of scope: `true`/`false` as union members / `?true` still mis-resolve as class alts in
  `VmCoerceToUnion` — pre-existing.)*
- ~~**`null` as standalone type** (8.2)~~ ✅ (§2) — bare `null` parses (represented as
  `MEMOBJ_NULL`) and is enforced null-only on returns and properties; `?null` still rejected
  (PHP does too).
- ~~**Traversable spread**~~ ✅ (§2) — `[...$iterator]` (and `f(...$it)` for variable/`new`
  operands) now walk the iterator protocol via `PH7_VmIteratorWalk`. *Pre-existing limitation
  (affects arrays too):* call-arg spread of a non-variable expression result (`f(...g())`,
  `f(...[1,2,3])`) is not expanded — see the §2 note.
- **Enums** (8.1) — `enum` keyword; `case Name;` / `case Name = value;`; backed-type
  consistency; hidden class implementing `UnitEnum`/`BackedEnum` (already declared,
  `vm.c:1079-1084`); static `cases()`/`from()`/`tryFrom()`; singleton semantics; block
  instantiation/inheritance; allow interfaces/traits/consts/methods; reject `__construct`. **L.**
- **`readonly` property** (8.1) + **`readonly` class** (8.2) — keyword; attribute on
  (promoted) properties; reject second write with `Error: Cannot modify readonly property`;
  class form applies to all declared props and forbids dynamic props. **M.**
- ~~**Typed class constants** (8.3)~~ ✅ (§2) — a type before the constant name parses (with a
  two-token disambiguation so the untyped `const NAME = …` path is untouched) and the computed
  value is enforced strictly at definition time. *Known cosmetic gap:* a `self`-typed constant
  renders `self` where PHP expands it to the class name (same §3.7 class as `iterable`).
- ~~**`final` constants** (8.1)~~ ✅ (§2) — `final const` (in either modifier order) parses and a
  child override raises the PHP-exact `Cannot override final constant` fatal.
- **Anonymous classes** (7.0) — in `new` compilation detect `class`, parse inline body
  (optional extends/implements/use), synthesize a hidden name, instantiate; support
  `new class(args) extends X {}`. **M.**
- **Attributes** (8.0) — the line-swallow hazard is fixed (§2): `#[ … ]` groups are now
  skipped as balanced trivia. Remaining: parse + store on functions/methods/classes/
  properties/params/consts; implement `\Attribute` with flags. Runtime retrieval depends
  on **Reflection** (§3.4). **M** for parse+store.

### 3.3 Object semantics / magic methods

- **`__sleep`/`__wakeup`, `__serialize`/`__unserialize`** — `serialize()`/`unserialize()`
  are currently aliased to JSON (`vm.c:15350`); needs a real serialization format with
  these hooks. **M.**
- **`__set_state`** (var_export round-trip), **`__debugInfo`** (var_dump). **S each.**
- **Override-compatibility checks with variance** (7.4) — there is currently **no check**
  (§3.1): any override is accepted, so "covariance" passes by accident alongside invalid
  redeclarations. Implement the PHP 7.4 rules — covariant returns, contravariant params —
  and emit the PHP-exact fatal for incompatible overrides. **M.**

### 3.4 Built-in classes / interfaces

> Built-in classes/interfaces are defined as **embedded PHP-source strings** compiled at VM
> init (`vm.c:1060+`). Pure-PHP class trees (SPL exceptions, JsonSerializable, much of the
> SPL hierarchy) can be added there cheaply — no C required.

- **`Closure` class** (M) — `bind`/`bindTo`/`fromCallable`/`call`. **Unblocks** first-class
  callables (§3.2) and `Closure::*`.
- ~~**`JsonSerializable`**~~ ✅ (§2) — interface declared + `json_encode` calls
  `jsonSerialize()`; exception propagation wired; surfaced & fixed `mixed` return type and
  throw-during-typed-return enforcement bugs.
- ~~**SPL exceptions**~~ ✅ (§2) — all 13 (RuntimeException, LogicException,
  InvalidArgumentException, OutOfRangeException, OutOfBoundsException, DomainException,
  RangeException, LengthException, BadFunctionCallException, BadMethodCallException,
  OverflowException, UnderflowException, UnexpectedValueException) added as embedded PHP
  source with the PHP-exact two-root hierarchy.
- **SPL data structures** (L) — SplStack, SplQueue, SplDoublyLinkedList, SplHeap/Min/Max,
  SplPriorityQueue, SplFixedArray, SplObjectStorage, SplObserver/SplSubject.
- **SPL iterators** (L) — ArrayIterator, ArrayObject, RecursiveIterator(Iterator),
  DirectoryIterator, FilesystemIterator, GlobIterator, CallbackFilterIterator, LimitIterator,
  AppendIterator, RegexIterator, EmptyIterator.
- **DateTime family** (L) — DateTime, DateTimeImmutable, DateInterval, DatePeriod,
  DateTimeZone, DateTimeInterface; unblocks `date_create`/`date_diff` aliases.
- **`WeakReference` / `WeakMap`** (M).
- **Reflection API** (XL) — ReflectionClass/Object/Method/Function/Parameter/Property/
  Type/NamedType/UnionType/IntersectionType/Attribute/Enum/ClassConstant/Extension/Generator.
  Touches every metadata structure. **Unblocks** attribute retrieval and CLI `--rf/--rc/--rm`.
- **PDO + driver(s)** (XL each) — see §6 for scope.

### 3.5 Standard library

- ~~**Array helpers**~~ ✅ (§2): `array_column`, `array_is_list` (8.1), `array_find`,
  `array_find_key`, `array_any`, `array_all` (8.4).
- **`hash` family** (M): `hash`, `hash_init/update/final`, `hash_hmac`, `hash_algos`,
  `hash_equals` + SHA-256/512. **Unblocks** `password_*` (M: `password_hash/verify/
  needs_rehash/get_info`, bcrypt/Argon2).
- ~~**Iterator helpers** (S): `iterator_to_array`, `iterator_apply`, `iterator_count`~~ ✅ (§2).
- ~~**JSON** (S): `json_last_error_msg`, `json_validate`~~ ✅ (§2); audit JSON constants
  (`JSON_INVALID_UTF8_*`, `JSON_THROW_ON_ERROR`, etc.) still open.
- **`filter_var` / `filter_input`** (M) + `FILTER_VALIDATE_*`.
- **`mb_*` multibyte** (L) — real `mb_strlen`/`mb_substr`/`mb_convert_encoding` (currently
  only `mb_strtolower`/`mb_strtoupper` aliases). See §6 for UTF-8-only policy.
- **Sessions** (L) — `session_*` functions + save-handler abstraction + `$_SESSION`
  population + cookie IDs (superglobal infra already present).
- **INI API** (L) — `ini_get/set/restore/get_all`, `get_cfg_var`; depends on **php.ini** (§3.6).
- ~~**`microtime(true)` / `gettimeofday` µs resolution**~~ ✅ (§2) — the float branch returned
  `(double)tm_sec`, dropping microseconds on every platform, and the non-`__UNIXES__` path had
  no sub-second source. Now returns `sec + usec/1e6`; the `microtime()` string is PHP-exact
  `0.dddddddd sec`; and a single overridable `PH7_CONFIG_CLOCK` hook lets embedders (the ESP32
  port via `esp_timer`) supply time without re-registering PHP functions.

### 3.6 Engine / CLI / SAPI

- ~~**`PHP_VERSION` + `PHP_MAJOR/MINOR/RELEASE_VERSION`, `PHP_VERSION_ID`, `PHP_EXTRA_VERSION`**~~
  ✅ (§2) — declared in `constant.c` from `PHP_COMPAT_*` macros (`ph7.h`).
- ~~**`phpversion()` + `php_sapi_name()`**~~ ✅ (§2) — `phpversion()` returns the PHP-compat
  version; `php_sapi_name()` returns `"cli"` / `"cli-server"`.
- **CLI `-l`** (S, ~80% there via `ph7_compile_file`), **`-i`** (S, `phpinfo()` exists),
  **`-a` REPL** (M), **`-m`** (S, once a module concept exists).
- **CLI `$_SERVER`** (S) — populate `SCRIPT_NAME`, `PHP_SELF`, `argv`, etc. on CLI startup.
- **`php.ini` subsystem** (L) — directive set, parser, lookup, `.user.ini`, `-d name=value`.
  **Unblocks** INI API (§3.5) and `--INI--` tests (§3.7).
- **Stream wrappers + sockets** (L) — `fsockopen`, `stream_socket_*`, `stream_context_create`,
  `stream_wrapper_register`; protocols `php://`, `data://`, `http://`. Builds on `net.c`.
- **Multi-threaded `-S` server** (L) — currently sequential accept-handle (`server.c:14`).
- **`pcntl_*` / `posix_*`** (M each).

### 3.7 Test infrastructure & error fidelity

**Runner status (re-worked 10 Jun 2026 — see §2):** two modes, neither able to truncate
silently:

- **In-process mode** (default, no `--target-executable`): each `--FILE--` is `include()`d
  in the runner's own process via an `ob_start`/`handle_error` wrapper that normalizes
  non-fatal error output (`Error [$errno]: … in %s on line %d`) identically across engines.
  This is fast (smoke ~4s) and is what `make test-smoke` / `test-smoke-compat` use; the
  smoke corpus' error-path expectations are written in this format and the corpus is
  curated to never `exit`/`die` or pollute the interpreter. If a test ever does kill the
  interpreter, the engine now lets shutdown callbacks run and the harness' abort guard
  prints `Bail out!` + exits nonzero (loud, not silent — see §2).
- **Direct mode** (`--target-executable BIN`): engine-native output; one child process per
  test. Used by the integration targets, the full-corpus baseline, and any test that
  legitimately exits/dies.
- **Direct-mode known failures: 24 of 2760** (full corpus). All one class — those smoke
  tests' `--EXPECTF--` encodes the normalized in-process handler format, while direct mode
  sees the engine's native output. They pass under the supported smoke target (in-process);
  the underlying *engine fidelity* gap is what matters (M, part of the audit below): native
  diagnostics omit the ` in %s on line %d` suffix and use wrong severity labels (e.g.
  `Error:` where PHP prints `Deprecated:` for `chr()`/`ord()` deprecations). Affected:
  `addcslashes`/`addslashes` null coercion, `array_chunk` float size, `array_fill`
  fractional, `array_flip` invalid values, `array_key_exists` float/null key,
  `array_sum` nested, `chr` range, `count` recursion, `ord` coercions.
- Newly noted fidelity gap for the audit: `var_dump(true)` prints `bool(TRUE)` (PHP:
  `bool(true)`); smoke tests avoid var_dump on booleans for this reason.
- **PHPT sections** — `--ENV--`/`--ARGS--`/`--STDIN--` (S); `--EXPECTREGEX--` (S — already
  parsed, just not compared); `--POST--`/`--GET--`/`--COOKIE--`/`--POST_RAW--` need a CGI
  harness (M); `--INI--` depends on php.ini (S once it lands). Not urgent: no current test
  uses any of them.
- **Error-message fidelity audit** (M) — diff against official PHP (severity labels,
  ` in %s on line %d` location suffix, param counts, unclosed-string line numbers,
  `Uncaught` formatting). The 24 known failures above are the seed corpus.
- **Deprecation / `error_reporting()` / `@` suppression** audits (S each).

### 3.8 Engine performance & embedding (from hardware benchmarking)

Surfaced by the first on-hardware profile of the engine (PHL on ESP32-S3, 11 Jun 2026;
`ports/esp32s3/bench/`, analysis in `results-*/ANALYSIS.md`). These are
platform-independent — the same curves reproduce on the host with more headroom — and the
fixes benefit the CLI, the `-S` server, and every embedder. The ESP32 port (`ESP32.md`)
*depends* on them but does not own them. (Two correctness consequences — silent OOM and
`preg_match` captures — are filed under §3.1; the items below are performance/architecture.)

- ~~**Quadratic string concatenation**~~ ✅ (§2) — `$s .= "..."` was O(n²) (host n=80k =
  11.57 s; on-chip 64 KB = 79 s) — the single biggest blocker to PHP-as-templating. The cause
  was *not* the SyBlob growth (`BlobPrepareGrow` already reallocs `nByte+mByte*2+16`,
  super-geometric) and `str_repeat`/`implode`/output-buffer were already linear; the cost was
  three full O(n) buffer copies per `.=` in `OP_CAT_STORE` (COW-dup of the read-only-aliased
  loaded lvalue + two `PH7_MemObjStore`/`SyBlobDup`). Fixed with an in-place append fast path
  that writes straight into the lvalue's owned slot buffer → amortized O(1) (n=80k 11.57 s →
  0.01 s; array-element and object-property forms too). The explicit `$s = $s . x` form stays
  O(n²) by design (documented).
- ~~**VM reuse across requests (`ph7_vm_reset` completeness)**~~ ✅ (§2) — `PH7_VmReset`
  now returns the VM to its post-`PH7_VmMakeReady` state (watermark-truncate the object pool
  + re-run the real init helpers), clearing every per-exec vector while preserving the
  compiled program. The `-S` server compiles each script once and reset-reuses it per
  request. **Unblocks** ESP32 B1.
- **Iterative bytecode execution / small native stack per frame** — `VmByteCodeExec`
  recurses in C once per PHP call frame (~800 B of native stack each), so deep PHP recursion
  overflows small embedder stacks (on ESP32's 16 KB task it panics at depth ≈20) and is
  bounded by the host C stack everywhere. A trampolined/iterative executor or heap-allocated
  frames removes the class of problem and decouples PHP recursion depth from the C stack.
  Large and invasive — tracked as a §6 candidate, not scheduled. **XL.**

---

## 4. Dependency map

Order should follow these edges, not raw effort:

- **`Closure` class** → first-class callables `f(...)`, `Closure::bind/call`.
- **Reflection API** → attribute retrieval, CLI `--rf/--rc/--rm`, parts of `-m`.
- **`php.ini` subsystem** → `ini_get/set`, `-d`, `--INI--` test section.
- **`hash` family** → `password_*`.
- **Traversable** (done) → `iterable` property type, Traversable spread, iterator helpers.
- **Embedded-PHP-source class pattern** (`vm.c:1060+`) → cheap SPL-exception /
  JsonSerializable / SPL-tree wins.
- **Attribute lexer fix** (done, §2) → safe to run real-world 8.x sources → attribute
  parse+store later.
- **`exit`/`die` VM unwind + abort guard** (done, §2) → the runner can no longer truncate
  silently, so every other item's regression coverage is trustworthy. **`PHP_BINARY`**
  (done) → a framework version-gate compat win alongside the §3.6 version constants.
- **VM-reset completeness (§3.8)** → compile-once / execute-many for the `-S` server and any
  embedder (ESP32 B1). Independent of the parser/stdlib work above; can land any time.

---

## 5. Roadmap

Principle: **correctness bugs → cheap high-frequency wins → dependency unblockers →
large subsystems → strategic/out-of-scope.**

**Tier 0 — Correctness hazards (ship first, S)**
*Previously shipped* (✅ `__invoke`/callback/constructor exception unwind · ✅ `HashmapMerge`
by-ref drop · ✅ multi-array `array_map` · ✅ float identity · ✅ non-numeric `--` ·
✅ array-literal arg collapse · ✅ `array_udiff` family exception swallow ·
✅ `#[…]` line-swallow lexer fix · ✅ sound-by-default test runner + `PHP_BINARY` ·
✅ `preg_match`/`preg_match_all` `$matches` for undefined vars (by-ref builtin auto-vivify) ·
✅ silent allocation failure (OOM → empty value + success; now a non-catchable fatal) ·
✅ `catch` block enclosing-scope access (was an isolated empty scope; now shares the
enclosing frame — fixes the misfiled "closure call in catch fails") ·
✅ `return` from inside a `catch`/`finally` (was silently dropped + fell through; now returns
from the enclosing function with finally-override and frame-safe unwinding)
— see §2). The override-signature-check gap (§3.1) is permissive-only and folded into the
Tier 2 covariance work. **All Tier 0 correctness hazards are now shipped** (the open §3.1
items — `(object)`-cast dynamic properties, override variance — are lower-frequency / folded
into Tier 2).

**Tier 1 — Cheap, high-frequency (S)**
~~`PHP_VERSION` constants + `phpversion()` + `php_sapi_name()`~~ ✅ (§2) ·
~~`array_column`/`array_is_list`/`array_find*`/`array_any`/`array_all`~~ ✅ (§2) ·
~~`json_last_error_msg`/`json_validate`~~ ✅ (§2) ·
~~`JsonSerializable`~~ ✅ (§2) · ~~SPL exceptions (embedded PHP)~~ ✅ (§2) ·
~~`iterator_to_array` family~~ ✅ (§2) ·
~~`iterable` property type · `true`/`false` return-type fix + standalone `null` type~~ ✅ (§2) ·
~~Traversable spread~~ ✅ (§2) · ~~typed + `final` class constants~~ ✅ (§2) · CLI `-l`/`-i` · CLI `$_SERVER` ·
`PHP_BINARY` in `-S` server VMs (CLI already done, §2) ·
~~`microtime(true)`/`gettimeofday` µs resolution~~ ✅ (§2).

**Tier 2 — Medium / unblockers (M)**
`Closure` class → first-class callables · `yield from` · keyed list destructuring ·
anonymous classes · `readonly` property + class · intersection + DNF types ·
override-compatibility checks with variance (no checks exist today, §3.3) ·
`hash` family → `password_*` · `__serialize`/`__unserialize`/`__sleep`/`__wakeup`/
`__set_state`/`__debugInfo` · `filter_var` · attribute parse + store · error-format audit
(seeded by the 24 known failures, §3.7) · `-a` REPL ·
~~quadratic string-concat → amortized-O(1) growth~~ ✅ (§2, §3.8) ·
~~VM-reset completeness → compile-once / execute-many for `-S` + embedders~~ ✅ (§2, §3.8).

**Tier 3 — Large subsystems (L)**
enums · DateTime family · SPL data structures · SPL iterators · `php.ini` → INI API ·
sessions · stream wrappers + sockets · real `mb_*` · `WeakReference`/`WeakMap`.

**Tier 4 — XL / strategic (decide via §6)**
Reflection API · PDO + a driver · cURL · FastCGI/FPM · `dl()` · opcache/JIT · SimpleXML/DOM.

---

## 6. Scope policy (pending ratification)

Given PHL's lightweight, embeddable, single-binary goal, the following need an explicit
decision before they sit on the roadmap. Recommended defaults:

| Decision | Recommended default |
|---|---|
| PHP version target | Target **PHP 8.x** semantics; report a PHP-compat version string. |
| Reflection | Ship a **minimal subset** (class/method/param names) first; full API later. |
| `mb_*` | **UTF-8-only** family, not full encoding support. |
| `dl()`, opcache, JIT, mod_php/FPM | **Out of scope** — bloat / breaks single-binary promise. |
| PDO | **SQLite only** if any driver ships. |
| Iterative VM executor (§3.8) | **Deferred** — large rewrite; until then embedders cap recursion depth (`PH7_VM_CONFIG_RECURSION_DEPTH`) — hitting the cap is now a clean non-catchable fatal on every call path (§2), never a panic — or enlarge the host C stack. |

---

## 7. Verification

This document's value is that its status column matches reality. Re-check it, don't assert it.

**Refresh the metrics** (paste results into the header & §1):
```sh
make
BIN=build/arm64-apple-darwin/full/phl
$BIN -r '
  echo "phpversion=", function_exists("phpversion")?phpversion():"(undefined)", "\n";
  echo "PHP_VERSION=", defined("PHP_VERSION")?PHP_VERSION:"(undefined)", "\n";
  echo "internal_functions=", count(get_defined_functions()["internal"]), "\n";
  echo "classes=", count(get_declared_classes()), "\n";
  echo "interfaces=", count(get_declared_interfaces()), "\n";
'
find tests -name '*.phpt' | wc -l
```

**Spot-check a status before trusting it** — a one-liner beats the prose:
```sh
$BIN -r 'echo class_exists("RuntimeException")?"yes":"no";'   # ✅ "yes" (§2)
$BIN -r '["k"=>$v]=["k"=>1]; echo $v;'                        # ❌ Unexpected token '=>'
$BIN -r 'echo str_contains("ab","b")?"yes":"no";'            # ✅ expected
```
Performance/embedding probes (§3.8):
```sh
$BIN -r 'preg_match("/(\d+)/","a12",$m); var_dump($m);'      # now ["12","12"] even for undefined $m (§2)
$BIN -r '$a=microtime(true);usleep(2000);echo microtime(true)-$a > 0 ? "us":"1s";'  # "1s" today (§3.5)
# concat is O(n²): n=20000 already takes seconds; n=200000 is ~3 min wall (realloc thrash). (§3.8)
time $BIN -r '$s="";for($i=0;$i<20000;$i++)$s.="x";echo strlen($s);'
```
Confirm both directions: that "Done" items really run **and** that "Missing" items really
fail. Beware of a trap class found this revision: a feature can "pass" because nothing is
checked at all (override signatures, §3.1). (A second trap — `#[…]` eating the rest of
its probe line — was fixed in §2.)

**Engine exit semantics** — must match real php (the §2 fix):
```sh
$BIN -r 'exit(3);'; echo $?                                       # 3 (was 0)
$BIN -r 'register_shutdown_function(fn()=>print("S\n")); die("x\n");'   # x then S
$BIN -r 'eval("die(\"d\\n\");"); echo "after\n";'                 # only d (no "after")
```

**Regression run** — smoke runs in-process (fast); a killing test bails out loudly:
```sh
make test                                  # smoke (in-process) + integration (direct)
make test-compat                           # the same, also under the real php binary
$BIN tests/phpt.php --target-executable "$PWD/$BIN"     # full corpus, engine-native output
# Baseline: 2745 ok · 24 not ok (all §3.7 error-format coupling; they pass under the
# in-process smoke target) · 17 skip of 2769.
$BIN tests/phpt.php --target-dir tests/phptrunner --file-extension diag
# Self-diagnostics: 002/004 fail by design; 008 (die) triggers "Bail out!" + exit 1.
```
After any fix, add coverage under `tests/ph7/001-smoke/` (in-process; normalized error
format, must pass under real php too, no `exit`/`die`) or `tests/ph7/002-integration/`
(engine-native output, may exit/die) and re-run. Error-path tests should run unguarded
against both `phl` and the real `php` binary for byte-for-byte parity.
