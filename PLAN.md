# PHL Gap Analysis vs Official PHP

This document enumerates gaps between PHL (built on PH7) and official PHP. Each section identifies a missing feature, the PHP version that introduced it (where applicable), evidence/verification notes, and a rough implementation effort estimate (S ≤1 day, M 2–5 days, L 1–2 weeks, XL 3+ weeks — all inclusive of tests).

PHL version analysed: **2.1.4** (build Apr 20 2026).
Binary: `build/arm64-apple-darwin/full/phl`.

**Effort rubric** (includes implementation + tests + edge cases):
- **S** (Small) – ≤1 day; mostly a single function/opcode/keyword with straightforward tests.
- **M** (Medium) – 2–5 days; touches multiple layers (lexer + parser + compiler + VM) or has non-trivial semantics.
- **L** (Large) – 1–2 weeks; cross-cutting feature (new keyword set, new class family, new subsystem).
- **XL** (Extra Large) – 3+ weeks; major subsystem or module.

---

## 1. Language Parser / Lexer Gaps

### 1.1 `**` (exponentiation) operator — PHP 5.6
**Status:** Missing. `echo 2 ** 10;` → `PHP Fatal error: '*': Missing/Invalid operand`.
**Evidence:** No entry in `aOpTable` (`src/ph7/parse.c`), no `EXPR_OP_POW` token, no `PH7_OP_POW` opcode.
**Scope:** Add token `TK_STAR_STAR`, operator entry with right-associativity and precedence above `*`, compile to new VM opcode, implement integer and float cases, `**=` compound assignment.
**Effort:** **S**

### 1.2 `**=` (power-assignment) — PHP 5.6
**Status:** Missing.
**Scope:** Follows `**`; adds compound-assign opcode.
**Effort:** **S** (bundled with 1.1)

### 1.3 `yield from` — PHP 7.0
**Status:** Missing. `yield from inner();` → `Unexpected token '('`.
**Evidence:** No `PH7_TKWRD_YIELD_FROM` token, no delegating-generator logic in `vm.c`.
**Scope:** Lex `from` after `yield`, emit opcode that iterates source generator/iterable/array and forwards values+keys, forward `send()`/`throw()`, capture return value. Handle nesting depth.
**Effort:** **M**

### 1.4 Nested short-array literals with `=>` — PHP 5.4
**Status:** **Broken**. `[["a" => 1], ["a" => 2]]` → `Short array: Missing closing bracket ']'`. Works with `array(array(...))`.
**Evidence:** Parser misinterprets inner `]` when `=>` is present inside. Reproduced in both `-r` and file form.
**Scope:** Fix bracket-depth tracking in `PH7_CompileShortArray` (`src/ph7/compile.c`) for nested keyed entries.
**Effort:** **S** (bug fix, important though — very common idiom)

### 1.5 Keyed list destructuring `["k" => $var]` — PHP 7.1
**Status:** Missing. `["name" => $n] = $data;` → `Syntax error: Unexpected token '=>'`.
**Scope:** Extend list/short-list compiler to accept key expressions, emit key-based lookup, allow nesting.
**Effort:** **M**

### 1.6 Array unpack (spread) with string keys — PHP 8.1 — **DONE**
**Status:** Implemented. `$b = ["c" => 3, ...$a]` now produces `["c" => 3, "a" => 1, "b" => 2]`. Integer keys are auto-renumbered, string keys preserved (later wins on collision), arrays of mixed key types behave as PHP 8.1 specifies. Both `[]` and `array(...)` syntaxes are supported, with arbitrary interleaving of literal entries and spreads at any nesting depth.
**Implementation:** Original PLAN framing was off. `PH7_OP_SPREAD` is the function-argument unpack opcode (`f(...$a)`); array literals went through a separate path that didn't handle `...` at all — the source array was inserted as a single entry under an auto-index. Fix introduces a transient stack flag `MEMOBJ_AUX_SPREAD` (`src/ph7/ph7int.h`) and a one-line opcode `PH7_OP_FLAG_SPREAD` (`src/ph7/vm.c`) that ORs the flag onto TOS. `GenStateCompileArrayBody` (`src/ph7/compile.c`) detects `PH7_TK_ELLIPSIS` at the start of an array entry's value tokens and emits the flag op after compiling the value. `PH7_OP_LOAD_MAP` checks the flag and dispatches to a new public wrapper `PH7_HashmapMerge` (`src/ph7/hashmap.c`) that delegates to the same static `HashmapMerge` already used by `array_merge()` — guaranteeing identical merge semantics. Function-argument spread is untouched.
**Validation rules:** `[k => ...$a]` and `[&...$a]` raise a Parse error matching PHP's wording (`syntax error, unexpected token "..."`). Spreading a non-array throws a catchable `\Error` (or `\TypeError` for objects) with PHP's exact message format `Only arrays and Traversables can be unpacked, X given`, where X uses PHP's value-name convention (`true`/`false` for booleans, class name for objects). Output is byte-for-byte identical to PHP for the catchable cases — see `tests/ph7/002-integration/error/short_array_spread_non_array.phpt`, which runs unguarded on both PHL and the real `php` binary.
**Out of scope (filed separately):** `Traversable` objects (iterators) cannot yet be spread — only arrays. This waits on the SPL/`Traversable` interface work tracked under item 6.2. See also 1.6a below for a pre-existing reference-propagation bug discovered during this work.

### 1.6a `HashmapMerge` drops by-reference entries — pre-existing
**Status:** **Broken**. `$x=5; $a=[&$x]; $b=[...$a]; $x=99;` leaves `$b[0]` as `5` in PHL; PHP keeps `$b[0]` bound to `$x` and reports `99`. Same bug affects `array_merge` (`$b = array_merge([], $a);` shows the identical detachment). `HashmapDuplicateNode` (`src/ph7/hashmap.c:1054`) does `sSafeVal = *pVal` and inserts by value, never consulting `MEMOBJ_REFERENCE`. This pre-dates the array-spread work but surfaces through it.
**Scope:** In `HashmapDuplicateNode`'s merge path, when the source node holds a by-ref value (`pVal->iFlags & MEMOBJ_REFERENCE`, or the node carries a foreign index), use `PH7_HashmapInsertByRef` with the same target index instead of duplicating the value. Verify simple-assign refs (`$d = $c;`) still behave correctly (they currently do).
**Effort:** **S**

### 1.7 First-class callable syntax `fn(...)` — PHP 8.1
**Status:** Missing. `$fn = strlen(...);` is interpreted as a call with a single `...` argument.
**Scope:** Detect lone `...` in an argument list and emit a Closure object referencing the callable (function/method/instance method).
**Effort:** **M** (needs Closure class — see 6.1)

### 1.8 Intersection types `A & B` — PHP 8.1
**Status:** Missing. `function f(A&B $x)` → `Invalid argument name`.
**Scope:** Lexer already has `&`; parser needs to treat `&` inside a type context as intersection join (not by-ref marker). Extend `ph7_vm_func_arg` with an intersection-alts container parallel to `aUnionAlts`. Verify at call/return time that value implements all listed interfaces/classes.
**Effort:** **M**

### 1.9 DNF types `(A&B)|C` — PHP 8.2
**Status:** Missing. No parsing of parenthesised type groups.
**Scope:** Parse parenthesised intersection groups inside union lists, extend type container to support a tree.
**Effort:** **M** (bundled with 1.8)

### 1.10 `iterable` pseudo-type — PHP 7.1
**Status:** Partial. Token is recognised but rejected with comment "The 'iterable' pseudo-type is not yet supported".
**Scope:** Accept `iterable` as `array|Traversable`; validate at call/return.
**Effort:** **S** (requires Traversable interface — see 6.6)

### 1.11 `true` / `false` / `null` as standalone types — PHP 8.2
**Status:** Missing.
**Scope:** Accept these identifiers in type contexts, check value equality on enforcement.
**Effort:** **S**

### 1.12 Enums (pure & backed) — PHP 8.1
**Status:** Missing. `enum Status { case Active; }` → `Syntax error: Unexpected token ';'`.
**Scope:** Major — new keyword `enum`, parse `case Name;` / `case Name = value;`, enforce backed-type consistency, generate hidden class extending a new `UnitEnum`/`BackedEnum` interface, provide static `cases()`, `from()`, `tryFrom()` methods, enforce singleton semantics, block instantiation, block inheritance, allow interfaces/traits/consts/methods on enums, reject magic methods like `__construct`.
**Effort:** **L**

### 1.13 `readonly` property modifier — PHP 8.1
**Status:** Missing. `public readonly int $x` → `Invalid argument name`.
**Scope:** Lexer keyword, parser handling as attribute on properties (including promoted), runtime check that rejects second write with `Cannot modify readonly property` (`Error`). Works with constructor promotion.
**Effort:** **M**

### 1.14 `readonly` class modifier — PHP 8.2
**Status:** Missing. `readonly class Point` → `Syntax error`.
**Scope:** Apply readonly to every declared property of the class, reject dynamic properties on readonly classes.
**Effort:** **S** (bundled with 1.13)

### 1.15 Typed class constants — PHP 8.3
**Status:** Missing. `const int FOO = 5;` → `Invalid constant name`.
**Scope:** Accept type declaration before constant name, enforce on initializer and on override in subclasses.
**Effort:** **S**

### 1.16 `final` constants — PHP 8.1
**Status:** Missing. `final const BAR = 5;` → `Unexpected token 'const', Expecting method declaration after 'final' keyword`.
**Scope:** Allow `final` before `const`, reject override in children.
**Effort:** **S**

### 1.17 Anonymous classes — PHP 7.0
**Status:** Missing. `new class { ... }` → `Unexpected token 'function'` (parser doesn't recognise `class` after `new`).
**Scope:** In `new` compilation, detect `class` keyword, parse inline class body (optional extends/implements/use), synthesize a hidden class name, instantiate. Support constructor arguments `new class(args) extends X { ... }`.
**Effort:** **M**

### 1.18 Attributes `#[Attribute]` — PHP 8.0
**Status:** **Partially broken.** Parser swallows `#[...]` (treated like a comment), so files compile, but there is no runtime reflection to retrieve attributes.
**Scope:** Store attributes on functions, methods, classes, properties, params, and constants. Implement `ReflectionAttribute`. Implement special `\Attribute` class with flags (TARGET_CLASS, etc.). (Reflection itself is a separate big gap — see 6.12.)
**Effort:** **M** for parse + storage. Runtime retrieval depends on Reflection (see 6.12).

### 1.19 String increment (`$a = "a"; $a++;` → `"b"`) — **DONE**
**Status:** Implemented. `PH7_OP_INCR` now detects non-numeric strings and applies Perl-style increment via `PH7_MemObjStringIncrement` (`src/ph7/memobj.c`): right-to-left walk with carry through `9→0`, `z→a`, `Z→A`; non-alphanumeric byte stops the carry without prepending; carry-out prepends `1`/`a`/`A` based on the last carried class; empty string becomes `"1"`. Numeric strings (`"5"`, `"1.5"`) still flow through the existing numeric path. Post-increment correctness is preserved by forcing pTos to own its blob (`SyBlobNullAppend`) before mutating pObj, since `PH7_MemObjLoad` aliases pTos's blob over pObj's via `SXBLOB_RDONLY`.
**Related fix:** none — the helper does not reclassify the result, so values like `"d9"++ → "e0"` stay strings even though `"e0"` would parse as numeric.
**Out of scope (filed separately):** `PH7_OP_DECR` on non-numeric strings still coerces to int; PHP leaves them unchanged. Track as a new item. Also, `"007"++` produces int `8` rather than string `"008"` because `PH7_MemObjToNumeric` parses leading-zero integers numerically — that's a numeric-classification bug, not an increment bug. See also 1.19a below for a pre-existing real-valued increment compaction bug discovered during this work.

### 1.19a Real-valued `$a++` collapses to int — pre-existing
**Status:** **Broken**. `$a = 1.0; $a++; var_dump($a);` prints `int(2)` in PHL; PHP prints `float(2)`. The `PH7_OP_INCR` numeric path calls `PH7_MemObjTryInteger` after incrementing a real, which converts exactly-integer reals back to int. PHP never compacts real types on `++`. The same bug almost certainly affects `PH7_OP_DECR` and any other site that calls `PH7_MemObjTryInteger` post-arithmetic.
**Note:** The call in `src/ph7/vm.c` `PH7_OP_INCR` reads `PH7_MemObjTryInteger(pTos)` rather than `pObj`, which looks like a typo but is a separate concern from the broader "should not compact at all" issue.
**Scope:** Audit every `PH7_MemObjTryInteger` call site and remove the ones in arithmetic ops where PHP would preserve the float type. Likely candidates: `PH7_OP_INCR`, `PH7_OP_DECR`, possibly `PH7_OP_ADD`/`SUB`/`MUL`/`DIV`.
**Effort:** **S** to fix, **M** if a sweeping audit uncovers more sites.

### 1.19b `PH7_OP_DECR` on non-numeric strings — pre-existing
**Status:** **Broken**. `$a = "abc"; $a--; var_dump($a);` produces `int(-1)` in PHL; PHP leaves it as `string(3) "abc"` (decrement is a no-op for non-numeric strings). The DECR opcode unconditionally calls `PH7_MemObjToNumeric` like INCR used to.
**Scope:** In `PH7_OP_DECR`, detect non-numeric strings via the same `VmStringWantsPerlIncr`-style check used by INCR and skip mutation. Numeric strings should still decrement. Note that PHP does not have a Perl-style string DECR; the op is simply a no-op for non-numeric strings.
**Effort:** **S**

---

## 2. Missing Magic Methods / Object Semantics

### 2.1 `__invoke` not wired up — **DONE**
**Status:** Implemented. `$obj($args)`, `call_user_func($obj, ...)`, `call_user_func_array($obj, [...])`, `usort`, `array_map`, `preg_replace_callback`, and any other site funnelling through `PH7_VmCallUserFunction` or checking `ph7_value_is_callable` now dispatch to `__invoke` with arguments and return value. Missing `__invoke` raises a catchable `Error: Object of type X is not callable`. Inherited / private / variadic / by-ref / default-valued parameters all work.
**Related fix:** `PH7_VmIsCallable` was tightened to match PHP semantics (object is callable iff its class declares `__invoke`; the previous behavior of invoking `__invoke` as a runtime predicate during `is_callable` was non-standard and silently rejected object callables in `usort`/`array_map`/`preg_replace_callback`). This is a behavior change for any embedder that relied on the old C-API `ph7_value_is_callable` semantics.
**Notes:** Named-argument forwarding (`$obj(name: $value)`) works — the call-site `VmCallArgMap` is threaded through `VmCallClassMethodWithMap`. Multi-array `array_map($cb, $a, $b)` is broken for *any* callable (pre-existing PHL bug, not specific to objects); track separately. Exceptions raised from inside `VmCallClassMethodWithMap`-routed dispatch (e.g., a `TypeError` from strict_types coercion of an `__invoke` arg, or from a `NEW` constructor) are not unwound to the caller's `try/catch`: the catch block runs but execution continues past the failed call. This is a pre-existing bug in PHL's exception model that also affects the `NEW` opcode under strict_types; track separately.

### 2.2 `__sleep` / `__wakeup` — PHP 5
**Status:** Missing. No hook in serialize/unserialize paths.
**Effort:** **S** (serialize itself is currently aliased to JSON — see 4.4)

### 2.3 `__serialize` / `__unserialize` — PHP 7.4
**Status:** Missing.
**Effort:** **S** (bundled with 2.2)

### 2.4 `__set_state` — PHP 5.1 (for `var_export`)
**Status:** Missing. `var_export` exists but doesn't round-trip through `__set_state`.
**Effort:** **S**

### 2.5 `__debugInfo` — PHP 5.6
**Status:** Missing. `var_dump` doesn't consult it.
**Effort:** **S**

### 2.6 Covariant return / contravariant parameter types — PHP 7.4
**Status:** Missing. Overrides must match exactly.
**Scope:** When validating method compatibility, allow subclass-tightened returns and subclass-widened params.
**Effort:** **M**

---

## 3. CLI / SAPI Gaps

### 3.1 `-a` interactive REPL
**Status:** Missing.
**Scope:** Line-buffered REPL with persistent VM state, readline wrapper (optional), handling of partial statements.
**Effort:** **M**

### 3.2 `-l` syntax-check-only
**Status:** Missing.
**Scope:** Compile and discard output, print `No syntax errors` or error location. Already 80% there via `ph7_compile_file`; just need a flag.
**Effort:** **S**

### 3.3 `-i` phpinfo on CLI
**Status:** Missing. `phpinfo()` function exists but no CLI switch.
**Effort:** **S**

### 3.4 `-m` list modules
**Status:** Missing (no module system — see 5.1).
**Effort:** **S** once module concept lands.

### 3.5 `--rf` / `--rc` / `--rm` Reflection CLI
**Status:** Missing. Needs Reflection API (6.12).
**Effort:** **S** on top of Reflection.

### 3.6 FPM / FastCGI SAPI
**Status:** Missing. `phl -S` is a single-threaded development server (`src/phl/server.c`).
**Scope:** Implement FastCGI protocol, process pool, worker lifecycle.
**Effort:** **XL**

### 3.7 Apache module (mod_php equivalent)
**Status:** Missing.
**Effort:** **XL**

### 3.8 Multi-threaded / concurrent HTTP server
**Status:** Missing. `server.c` is sequential accept-handle loop.
**Effort:** **L**

### 3.9 CLI superglobals (`$_SERVER` in CLI mode)
**Status:** Missing. Only `$argv`/`$argc` and `$_ENV` populated in CLI. Official PHP also exposes `$_SERVER['SCRIPT_NAME']`, `argv`, `PHP_SELF`, etc.
**Scope:** Populate minimal `$_SERVER` on CLI startup.
**Effort:** **S**

---

## 4. Standard Library — Missing Functions

PHL exposes **475 internal functions** (verified via `get_defined_functions()`). Official PHP exposes ~3000. The gaps below are the high-value ones.

### 4.1 PHP 8 string helpers — PHP 8.0
Missing: `str_contains`, `str_starts_with`, `str_ends_with`.
**Verification:** All three return "Call to undefined function" warnings.
**Effort:** **S** (trivial wrappers around existing SyString helpers).

### 4.2 Array helpers
Missing: `array_column`, `array_is_list` (8.1), `array_any` / `array_all` / `array_find` / `array_find_key` (8.4).
**Verification:** Each returns `Call to undefined function`.
**Effort:** **S** per function (total **M**).

### 4.3 Secure random — PHP 7.0
Missing: `random_int`, `random_bytes`.
**Scope:** Use platform CSPRNG (`getrandom`, `/dev/urandom`, `CryptGenRandom`). Currently only `rand`/`mt_rand` are available.
**Effort:** **S**

### 4.4 Hashing extension — PHP 5.1
Missing: `hash`, `hash_init`, `hash_update`, `hash_final`, `hash_hmac`, `hash_algos`, `hash_equals`. Only `md5`, `sha1`, `crc32` exist as dedicated functions.
**Scope:** Wrap existing SHA/MD5 primitives in a unified hash API; add SHA-256, SHA-512 via a new implementation.
**Effort:** **M**

### 4.5 Password API — PHP 5.5
Missing: `password_hash`, `password_verify`, `password_needs_rehash`, `password_get_info`.
**Scope:** Requires bcrypt (and ideally Argon2). Depends on 4.4.
**Effort:** **M**

### 4.6 Sessions
Missing: `session_start`, `session_id`, `session_destroy`, `session_regenerate_id`, `session_write_close`, `$_SESSION` population, file-based session handler, cookie-based session IDs.
**Scope:** Full session subsystem including save handler abstraction.
**Effort:** **L**

### 4.7 INI API
Missing: `ini_get`, `ini_set`, `ini_restore`, `ini_get_all`, `get_cfg_var`, INI file loading, per-directory `.user.ini`.
**Scope:** Build INI storage + parser for `php.ini`; attach to engine config. Engine currently has no INI concept (see 5.2).
**Effort:** **L**

### 4.8 Sockets / streams
Missing: `fsockopen`, `pfsockopen`, `stream_socket_client`, `stream_socket_server`, `stream_context_create`, `stream_wrapper_register`, `stream_filter_*`, `stream_select`.
**Scope:** Build on `src/ph7/net.c` (internal only); surface as user-facing API; plumb stream wrapper protocol (`http://`, `ftp://`, `php://`, `data://`).
**Effort:** **L**

### 4.9 cURL extension
Missing: All `curl_*` functions (`curl_init`, `curl_setopt`, `curl_exec`, `curl_close`, `curl_multi_*`).
**Scope:** Build against libcurl or a minimal HTTP client; adds external dependency.
**Effort:** **L**

### 4.10 Generator interop helpers
Missing: `iterator_to_array`, `iterator_apply`, `iterator_count`.
**Effort:** **S**

### 4.11 Date/Time helpers
Missing beyond `date`/`time`: `DateTime` class family (see 6.3). `date_create`, `date_format`, `date_diff` are aliases that depend on that class.
**Effort:** Bundled with 6.3.

### 4.12 JSON helpers
Missing: `json_last_error_msg` (PHP 5.5), `json_validate` (PHP 8.3). JSON constants (`JSON_UNESCAPED_UNICODE`, `JSON_PRETTY_PRINT`, `JSON_THROW_ON_ERROR`, etc.) — need to verify which are accepted.
**Effort:** **S**

### 4.13 Output encoding helpers
Missing: `mb_*` multibyte string family (PHL has ad-hoc UTF-8 helpers but no `mb_strlen`, `mb_substr`, `mb_convert_encoding`, etc.).
**Scope:** New encoding-aware string module.
**Effort:** **L**

### 4.14 Reflection CLI-level helpers
Missing: full Reflection API (see 6.12).

### 4.15 Filter extension
Missing: `filter_var`, `filter_input`, `FILTER_VALIDATE_*`.
**Effort:** **M**

### 4.16 Miscellaneous commonly-used functions
Missing: `array_column`, `preg_grep`, `preg_filter`, `metaphone`, `similar_text` (verify), `soundex` (exists). `register_tick_function`, `register_tick_function` (ticks are parsed but no-op per `declare(ticks=)`).
**Effort:** **S** per entry.

### 4.17 `Closure` static methods missing
Missing: `Closure::bind`, `Closure::fromCallable`, `Closure::bindTo`, `Closure::call`.
**Verification:** `Closure` class isn't even defined (see 6.1).
**Effort:** Bundled with 6.1.

---

## 5. Engine Subsystems

### 5.1 Dynamic extensions / `dl()`
**Status:** Missing. No shared-object loader.
**Scope:** Introduce an extension module format (C ABI), a loader (`dlopen`/`LoadLibrary`), a manifest mechanism, and a registration API. Most PHP apps depend on this, but it fundamentally breaks PHL's "embeddable single binary" promise — decide in-scope or out-of-scope.
**Effort:** **XL**

### 5.2 `php.ini` configuration file
**Status:** Missing. No INI parsing pipeline, no engine-level config store.
**Scope:** Define supported directives, parser, lookup, `.user.ini` merging, command-line `-d name=value`. Blocks 4.7.
**Effort:** **L**

### 5.3 Opcache / bytecode caching
**Status:** Missing. Every request/run recompiles.
**Scope:** Serialize compiled bytecode to a cache store (shared memory, mmap'd files). Not usually needed for embedded but is visible in `phpinfo()`.
**Effort:** **XL**

### 5.4 JIT
**Status:** Missing. Not a priority for embedded use.
**Effort:** **XL** (likely out-of-scope for PHL's goals).

### 5.5 Autoloading — `__autoload` / `spl_autoload_register`
**Status:** Partially supported. `spl_autoload_register` exists, but no PSR-4 convention helper. Verify that `__autoload` magic function is still honored (deprecated in 7.2 / removed in 8.0, so PHL is PHP-8-consistent here).
**Effort:** N/A (already there); PSR-4 is user-code concern.

### 5.6 PHP version reporting
**Status:** **Broken**. `PHP_VERSION` constant is **not defined** (`defined("PHP_VERSION")` → false). `PHP_MAJOR_VERSION`, `PHP_MINOR_VERSION`, `PHP_RELEASE_VERSION`, `PHP_VERSION_ID`, `PHP_EXTRA_VERSION` also missing. `PHP_INT_MAX` exists.
**Effort:** **S** — declare the constants in `src/ph7/constant.c` at init.

### 5.7 Engine self-identification in `phpversion()`, `php_sapi_name()`
**Status:** Needs audit. `phpversion()` returns PHL engine version, not a PHP spec version — may break frameworks that gate on version.
**Effort:** **S** — add a PHP compatibility version string.

### 5.8 Stream wrappers
**Status:** Missing (`stream_wrapper_register` absent). Blocks `file_get_contents("http://...")`, `php://memory`, `php://temp`, `data://`.
**Effort:** **L** (bundled with 4.8).

### 5.9 Process control — `pcntl_*`
**Status:** Missing entirely.
**Effort:** **M**

### 5.10 POSIX functions — `posix_*`
**Status:** Missing.
**Effort:** **M**

---

## 6. Built-in Classes / Interfaces

Verified `get_declared_classes()` output: **14 classes** (`stdClass`, `Generator`, `Fiber`, `Directory`, `ErrorException`, `DivisionByZeroError`, `ArithmeticError`, `AssertionError`, `FiberError`, `ValueError`, `ArgumentCountError`, `TypeError`, `Error`, `Exception`).
Verified `get_declared_interfaces()`: **4 interfaces** (`Serializable`, `IteratorAggregate`, `Iterator`, `Throwable`).

### 6.1 `Closure` class
**Status:** Missing. Closures (the language feature) work, but the class isn't declared; `Closure::bind`, `Closure::fromCallable`, `Closure::call` all fail.
**Effort:** **M**

### 6.2 `ArrayAccess`, `Countable`, `Stringable`, `Traversable`, `UnitEnum`, `BackedEnum` interfaces
**Status:** Implemented for the dispatch surface listed below. `interface_exists()` returns true for all six; `Iterator`/`IteratorAggregate` now `extends Traversable`; `Stringable` is auto-implemented for any class declaring `__toString` (including the built-in `Exception`/`Error` classes).
**Implemented:**
- `ArrayAccess` — `$obj[$k]` (offsetGet), `$obj[$k]=$v` and `$obj[]=$v` (offsetSet), `isset($obj[$k])` (offsetExists), `unset($obj[$k])` (offsetUnset). VM hooks live in `PH7_OP_LOAD_IDX`/`PH7_OP_STORE_IDX` (vm.c). Compiler routes `isset` and `unset` arguments via new iP2 codes 4 and 5.
- `Countable` — `count($obj)` dispatches to `->count()`.
- `Stringable` — auto-added to any class with `__toString`, at compile time.
- `Traversable` — empty marker; `Iterator`/`IteratorAggregate` extend it; `instanceof Traversable` walks the parent-interface chain (`PH7_VmInstanceOf`).
- `UnitEnum`/`BackedEnum` — declared as marker interfaces; full enum support is §1.12.
**Out of scope (deferred):**
- Nested ArrayAccess writes (`$obj['a']['b'] = 1`) — requires ref-semantic offsetGet return.
- ArrayAccess `STORE_IDX_REF` (`$x =& $obj[$k]`) — runtime path is defended (throws "Cannot assign by reference to overloaded object") but the compiler rejects it earlier as "Reference operator require a variable not a constant", so the runtime guard is unreachable from valid PHP source.
**Effort:** **M** (done).
**Notes:**
- `empty($obj[$k])` routes through `offsetExists` first, then `offsetGet` only when the key exists. New iP2=6 code added.
- Subscripting a non-ArrayAccess object now throws a real `Error` (`Cannot use object of type X as array`) for read, write, isset, unset, and empty contexts — matches PHP. New `VmThrowFromVm` helper near `PH7_VmThrowException` factors the throw machinery (Error class lookup, instance construct, `VmThrowException`).

### 6.3 DateTime class family — PHP 5.2+
**Status:** Missing. `new DateTime()` → "Class 'DateTime' is not defined".
**Scope:** `DateTime`, `DateTimeImmutable`, `DateTime::createFromFormat`, `DateInterval`, `DatePeriod`, `DateTimeZone`, `DateTimeInterface` interface.
**Effort:** **L**

### 6.4 SPL data structures
**Status:** All missing. `new SplStack()` → "Class 'SplStack' is not defined". Confirmed: `SplStack`, `SplQueue`, `SplDoublyLinkedList`, `SplHeap`, `SplMinHeap`, `SplMaxHeap`, `SplPriorityQueue`, `SplFixedArray`, `SplObjectStorage`, `SplObserver`/`SplSubject`.
**Effort:** **L**

### 6.5 SPL iterators
**Status:** Missing. `ArrayIterator`, `ArrayObject`, `RecursiveIterator`, `RecursiveIteratorIterator`, `DirectoryIterator`, `FilesystemIterator`, `RecursiveDirectoryIterator`, `GlobIterator`, `CallbackFilterIterator`, `LimitIterator`, `AppendIterator`, `RegexIterator`, `EmptyIterator`.
**Effort:** **L**

### 6.6 SPL extension exceptions
**Status:** Missing. `RuntimeException`, `LogicException`, `InvalidArgumentException`, `OutOfRangeException`, `OutOfBoundsException`, `DomainException`, `RangeException`, `LengthException`, `BadFunctionCallException`, `BadMethodCallException`, `OverflowException`, `UnderflowException`, `UnexpectedValueException`. Only the PHP-core ones (`Exception`, `Error`, etc.) exist.
**Verification:** `class_exists("RuntimeException")` → false.
**Effort:** **M** (thin — just the class hierarchy)

### 6.7 `WeakReference` (PHP 7.4), `WeakMap` (PHP 8.0)
**Status:** Missing.
**Effort:** **M**

### 6.8 `JsonSerializable` interface
**Status:** Missing. `json_encode` doesn't consult it.
**Effort:** **S**

### 6.9 SimpleXML / DOMDocument / XMLReader / XMLWriter
**Status:** Missing. Only procedural `xml_parser_*` SAX API present.
**Effort:** **L** per family (so **XL** total).

### 6.10 PDO (PHP 5.1)
**Status:** Missing entirely. `class_exists("PDO")` → false.
**Scope:** PDO interface + drivers (SQLite, MySQL, PostgreSQL, ODBC). Each driver is itself a large project.
**Effort:** **XL** per driver.

### 6.11 `mysqli`, `pgsql`, `sqlite3` extensions
**Status:** Missing.
**Effort:** **XL** each.

### 6.12 Reflection API — PHP 5
**Status:** Missing. `class_exists("ReflectionClass")` → false.
**Scope:** `Reflection`, `ReflectionClass`, `ReflectionObject`, `ReflectionMethod`, `ReflectionFunction`, `ReflectionFunctionAbstract`, `ReflectionParameter`, `ReflectionProperty`, `ReflectionType`, `ReflectionNamedType`, `ReflectionUnionType`, `ReflectionIntersectionType`, `ReflectionAttribute`, `ReflectionEnum`, `ReflectionClassConstant`, `ReflectionExtension`, `ReflectionGenerator`, `ReflectionReference`. Touches every metadata structure in the engine.
**Effort:** **XL**

### 6.13 PSR `Random\*` classes — PHP 8.2
Missing: `Random\Randomizer`, `Random\Engine\*`, `Random\IntervalBoundary`.
**Effort:** **L** (needs 4.3 first).

---

## 7. Superglobals / Request Handling

### 7.1 `$_SESSION`
**Status:** Missing; blocked by 4.6.

### 7.2 `$_FILES` file uploads in `-S` server
**Status:** Server parses multipart/form-data? Needs verification. No upload handler in `src/phl/server.c`.
**Effort:** **M**

### 7.3 `$_COOKIE` round-trip with `setcookie`
**Status:** `setcookie` exists; verify header emission order and `SameSite`/`Secure`/`HttpOnly` support.
**Effort:** **S** (audit + fill gaps).

### 7.4 Superglobals under CLI (see 3.9)

---

## 8. Test Infrastructure / PHPT Compatibility

The test runner at `tests/phpt.php` declares support for `--TEST--`, `--DESCRIPTION--`, `--CREDITS--`, `--SKIPIF--`, `--FILE--`, `--EXPECT--`, `--EXPECTF--`, `--EXPECTREGEX--`, `--CLEAN--`. It marks as **unimplemented**: `--POST--`, `--POST_RAW--`, `--GET--`, `--COOKIE--`, `--STDIN--`, `--INI--`, `--ARGS--`, `--ENV--`.

### 8.1 `--INI--` test section
**Status:** Missing. Depends on 5.2.
**Effort:** **S** once 5.2 lands.

### 8.2 `--POST--` / `--GET--` / `--COOKIE--` / `--POST_RAW--`
**Status:** Missing. Requires CGI-mode execution harness.
**Effort:** **M**

### 8.3 `--ENV--`, `--ARGS--`, `--STDIN--`
**Status:** Missing.
**Effort:** **S**

### 8.4 `--CGI--` mode
**Status:** Missing. No CGI SAPI shim.
**Effort:** **M**

---

## 9. Error Reporting / Format Fidelity

### 9.1 Error message format audit
**Status:** Mostly compatible (`Warning:`, `Notice:`, `PHP Fatal error:  Uncaught`). Needs a diff run against official PHP to identify subtle differences (function parameter counts, unclosed-string line numbers, etc.).
**Effort:** **M** (audit + tightening).

### 9.2 Deprecation notices
**Status:** `E_DEPRECATED` constant exists; need to verify engine actually emits deprecations for e.g. implicit nullable params.
**Effort:** **S** (audit).

### 9.3 Error levels behaviour under `error_reporting()`
**Status:** Function exists; verify suppression via `@` operator.
**Effort:** **S** (audit).

---

## 10. PHP Version Target Table

Features grouped by which PHP version they come from, to help plan which versions PHL targets.

| PHP Version | Features PHL lacks |
|-------------|--------------------|
| 5.3 | (baseline — fully covered) |
| 5.4 | Nested short-array `=>` parser bug (1.4); traits (present); short echo (verify) |
| 5.5 | `finally` (present); generators (present); password API (4.5); `::class` (present); `foreach list()` (present) |
| 5.6 | `**` operator (1.1, 1.2); variadic (present); argument unpacking (present); constant expressions (present) |
| 7.0 | `yield from` (1.3); anonymous classes (1.17); `<=>` (present); `??` (present); scalar & return types (present); group use (verify); `Throwable` (present) |
| 7.1 | `iterable` pseudo-type (1.10); keyed list destructuring (1.5); class constant visibility (present); multi-catch (present); nullable types (present); void return (present) |
| 7.2 | Object type-hint (present); `HashContext` class (4.4) |
| 7.3 | Heredoc dedent (verified present); trailing commas in calls (present); JSON_THROW_ON_ERROR (4.12) |
| 7.4 | Typed properties (present); arrow functions (present); spread in array (partially — 1.6); `??=` (present); covariant returns (2.6); `WeakReference` (6.7); `__serialize`/`__unserialize` (2.3); numeric literal separators (present) |
| 8.0 | Named arguments (present); union types (present); match (present); nullsafe (present); constructor promotion (present); `throw` as expression (present); attributes (1.18 — parse OK, no retrieval); first-class syntax (1.7); `str_contains` family (4.1); `Stringable` (6.2); `WeakMap` (6.7); `ValueError` (present); `$object::class` (present) |
| 8.1 | Enums (1.12); `readonly` property (1.13); intersection types (1.8); `never` return (present); first-class callables (1.7); `array_is_list` (4.2); array unpack string keys (1.6); new-in-initializer (**works**); `Fiber` (present); pure intersections; `final` const (1.16); tentative return types |
| 8.2 | `readonly` class (1.14); DNF types (1.9); `true`/`false`/`null` as types (1.11); `Random\*` classes (6.13); deprecate dynamic properties |
| 8.3 | Typed class constants (1.15); `#[Override]` attribute; `json_validate` (4.12); `Randomizer::getBytesFromString` |
| 8.4 | `array_any`/`array_all`/`array_find` (4.2); property hooks; asymmetric visibility; new `Request` parser hooks |

---

## 11. Prioritised Roadmap (suggested)

This ordering favours high user-value, low-effort items first, then infrastructure that unblocks many features.

**Tier 1 — Quick wins (S, total ≈ 1 week)**
1. `**` and `**=` operators (1.1, 1.2)
2. `str_contains` / `str_starts_with` / `str_ends_with` (4.1)
3. `array_column`, `array_is_list` (4.2)
4. `random_int` / `random_bytes` (4.3)
5. `PHP_VERSION` constant + related (5.6)
6. `__invoke` dispatch bug (2.1)
7. String-increment bug (1.19)
8. Nested short-array parsing bug (1.4)
9. Array unpack with string keys (1.6)
10. `-l` lint flag, `-i` info flag (3.2, 3.3)
11. CLI `$_SERVER` minimal fill (3.9)
12. `iterable` pseudo-type (1.10)
13. `true`/`false`/`null` as types (1.11)
14. `final` constants, typed class constants (1.15, 1.16)
15. SPL exceptions hierarchy (6.6)
16. `JsonSerializable`, `json_last_error_msg` (6.8, 4.12)

**Tier 2 — Medium (M, total ≈ 6–10 weeks)**
17. `yield from` (1.3)
18. Keyed list destructuring (1.5)
19. Anonymous classes (1.17)
20. `readonly` property (1.13) + `readonly` class (1.14)
21. Intersection types (1.8) + DNF (1.9)
22. Attributes parse + store (1.18) — runtime retrieval waits on Reflection
23. `Closure` class + `bind`/`fromCallable` (6.1) → enables first-class callables (1.7)
24. `ArrayAccess`/`Countable`/`Stringable`/`Traversable` interfaces (6.2)
25. Covariant / contravariant method compatibility (2.6)
26. `hash` family (4.4) → `password_*` (4.5)
27. `iterator_to_array` + iterator helpers (4.10)
28. `__serialize`/`__unserialize`/`__sleep`/`__wakeup`/`__debugInfo` (2.2–2.5)
29. `filter_var` (4.15)
30. Error-message fidelity audit (9.1)
31. `-a` REPL (3.1)

**Tier 3 — Large (L, total ≈ 3–6 months)**
32. Enums (1.12)
33. DateTime class family (6.3)
34. SPL data structures (6.4)
35. SPL iterators (6.5)
36. `php.ini` subsystem (5.2) → `ini_get`/`ini_set` (4.7)
37. Sessions (4.6)
38. Stream wrappers + sockets (4.8, 5.8)
39. `mb_*` multibyte (4.13)
40. `WeakReference`/`WeakMap` (6.7)

**Tier 4 — Extra Large (XL, total ≈ 1 year+ / strategic decisions)**
41. Reflection API (6.12)
42. PDO + at least one driver (6.10)
43. cURL (4.9)
44. FastCGI / FPM SAPI (3.6)
45. Dynamic extension loader `dl()` (5.1) — may be out-of-scope for embeddable engine
46. Opcache / JIT (5.3, 5.4) — likely out-of-scope
47. SimpleXML / DOM (6.9)

---

## 12. Out-of-scope Candidates (decide up-front)

Given PHL's "lightweight, embeddable, single binary" goal, some features may be deliberately omitted:

- **Opcache, JIT** (5.3, 5.4) — bloat for embedded use.
- **Dynamic extension loader** (5.1) — breaks single-binary promise.
- **mod_php / FPM** (3.6, 3.7) — outside embeddable scope.
- **Full PDO driver set** — at most bundle SQLite.
- **mbstring extension** — may ship UTF-8-only.
- **Full Reflection** — a minimal subset (class/method name, parameters) may satisfy most frameworks.

These need an explicit policy decision before the roadmap is frozen.

---

## 13. Verification notes

Every claim marked "Broken" or "Missing" above was validated by running the `phl` binary with a targeted test case. The commands used appear in this document's history. Re-running these after each fix is the basic regression check; fuller coverage should be added to `tests/ph7/001-smoke/` and `tests/ph7/002-integration/` — which together already contain several hundred PHPT files.
