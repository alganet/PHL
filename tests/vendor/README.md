# tests/vendor — the ecosystem gate

Real-world PHP projects, each run under **php** and under **phl**, required to answer the
same thing. Where a project's parity with php used to be a number somebody wrote down after
a hand-run, it is now a committed baseline: a new divergence fails a gate instead of waiting
for someone to notice a paragraph has gone stale.

```sh
tests/vendor/run.sh                 # every project
tests/vendor/run.sh monolog twig    # some
tests/vendor/run.sh --list
tests/vendor/run.sh --run-only      # skip fetch/install/patch, just re-run the suites
tests/vendor/run.sh --accept        # write the current diffs as the new baselines
```

It is **off the Makefile and off CI** for now: it needs the network, a real `composer`, and
several minutes to run. Run it by hand before a release and after any change to the
compiler, the error surface or a shipped extension.

> ### ⚠ The `phpstan` step forks EIGHT workers, and that OOMs a loaded box
>
> Until 29 Aug 2026 respect-validation's `phpstan` step died in under a second, at
> phpstan.phar's own bootstrap, and its baseline was that death. Three engine defects
> shipped that day and it now starts a REAL analysis — and phpstan's default
> `parallel: maximumNumberOfProcesses` is **8**, so that is eight more `phl` processes,
> each carrying this engine's standing ~3.3x memory over php. It took this 30 GB box down
> twice, and a per-process `ulimit -v` cannot hold it: the limit applies to each child, not
> to their sum.
>
> **Bound it before running this project again.** Either force a single process
> (`--debug` on the phpstan command line does that, or a `parallel:
> maximumNumberOfProcesses: 1` override in the project's `phpstan.neon.dist`), or run the
> gate with the other projects only (`tests/vendor/run.sh monolog php-parser twig`) while
> the step's real cost is being measured. The baseline for that step is stale until
> somebody does.

Every verdict line carries what the step COST, per engine — `PARITY [php 9s phl 61s]` — and
each project and the run as a whole print their total. Those numbers are not part of the
answer (they are never diffed and never a baseline); they are there because a gate whose
wall-clock nobody can see is a gate that quietly stops being run. It was worth having: one
step was 98% of a whole run and nothing printed said so.

## What is committed, and what is not

**Not committed:** a single line of any project's source. Each project is a pinned git ref
plus a `composer.lock` we own; `run.sh` clones into `build/tests-vendor/` and installs from
that lock, so every run gets the same tree and nothing vendored ends up in this repository.

**Committed**, per project, under `projects/<name>/`:

| file | what it is |
|---|---|
| `manifest` | `REPO`, `REF`, and optionally `COMPOSER_FLAGS`, `PREPARE`, `NORMALIZE` — each with a comment saying WHY it is needed |
| `composer.lock` | the dependency graph the measurement is of. Written on first install; commit it |
| `steps` | `<id><TAB><command>`, one per line, run from the checkout root. The command names the engine as `$PHL_VENDOR_ENGINE` (unquoted — it carries ini flags) |
| `patches/*.patch` | applied after the install. **See the patch policy below** |
| `expected/<id>.diff` | the accepted php↔phl divergence for that step |

## The verdict

For each step the runner captures both engines' output and exit status, normalizes what is
genuinely per-run (colour, paths, wall-clock, memory, PHPUnit's seed, and whatever
`NORMALIZE` names), and diffs php against phl. That diff is compared to
`expected/<id>.diff`:

* **PARITY** — the diff is exactly the committed baseline (empty, or a known set of rows).
* **DIVERGED** — it is not. The gate fails. Either the engine changed, or the answer changed
  on purpose and the baseline needs review and `--accept`.
* **CRASHED** — the engine died on a signal. **This fails the gate no matter what the
  baseline says**, and cannot be accepted: "we already knew it segfaults" is exactly how a
  crash stops being work.

`error_reporting` is pinned to E_ALL on both sides, as a number the engines are asked for
and required to agree on — this box's `php.ini` masks `E_DEPRECATED`, and an unpinned run
reports php-correct deprecations as PHL divergences by the hundred.

A baseline is a LEDGER, not a pass. Every line in `expected/` is a divergence from php that
somebody decided to live with; read them, and prefer emptying one to growing it.

## The patch policy — the only accepted patch strategy

**The non-deprecated policy:** PHL targets PHP 8.5's **non-deprecated** surface — what php
merely deprecates, PHL removes or refuses, so it fails loudly instead of silently
half-working. Real projects still write those spellings, and a project that dies on one is a
project we cannot measure at all — which is how `doctrine/couchdb` cost monolog its whole
suite past 17%, and how `chr()`'s codepoint cost php-parser four errors it could never pass.

**A patch may only take a project OFF a surface the non-deprecated policy refuses because
php deprecates it.**
That is the entire licence. It keeps the policy and still gets the suite to the end.

Not allowed, ever:

* working around an **engine defect** — that gets fixed in the engine, and until it is, it
  belongs in a baseline where it is visible;
* working around a **scope cut that is not a deprecation** — the timezone database, `mb_*`'s
  encodings, fileinfo's magic database. Those are measured, not papered over;
* making a suite quieter, faster, or greener for any other reason.

`run.sh` enforces this mechanically. Every patch must begin with three headers before its
diff, and a patch without them, or with a reason that is not a deprecation surface, is a
hard error:

```
PHL-Patch-Reason: chr-out-of-range
PHL-Patch-Kind: rewrite
PHL-Patch-Note: one line -- what it does, and what it costs.
```

`PHL-Patch-Kind` is the honesty field, and every run prints the tally:

* **`rewrite`** — changes NO behaviour under php. `T $x = null` → `?T $x = null`;
  `chr($n)` → `chr($n % 256)`; `E_ALL | E_STRICT` → `E_ALL`; `strlen($x)` → `strlen($x ?? '')`;
  `ord($chr)` → `ord($chr[0])`. php reads the two spellings the same way. Prefer this always.
* **`adapt`** — the project still does the thing, spelled without the deprecated construct,
  but the two are not identical under php. A test that fakes a failed write with
  `E_USER_ERROR` where any error would do becomes `E_USER_WARNING`.
* **`cut`** — the project's own coverage SHRINKS, because the test asks for the deprecated
  answer by name and there is no other spelling of it. Legal, and the note must say what is
  no longer tested.

One patch per **package**: a patch spanning two packages cannot be re-applied after
`composer reinstall` of one of them, and the runner will refuse it as neither applied nor
appliable.

`tools/fix-implicit-nullable.php` generates the `implicit-nullable-param` rewrite over a
whole tree (`--dry-run` first to size it). It splices a `?` at the type's own byte offset
rather than pretty-printing, so the diff is one character per parameter. Use it rather than
hand-editing: this is the deprecation surface projects hit by the dozen, and by hand is how a
"deprecation patch" quietly becomes an edit nobody reviewed.

## Adding a project

1. `mkdir -p projects/<name>/patches projects/<name>/expected`, write `manifest` and `steps`.
2. `tests/vendor/run.sh --prepare <name>` — it clones, installs, and writes the lock for you.
   **Commit that lock.**
3. `tests/vendor/run.sh <name>` and read every divergence. Patch only what the policy above
   allows; everything else you either fix in the engine or accept into the baseline.
4. `tests/vendor/run.sh --accept <name>`, review `expected/`, commit.
