#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause
#
# tests/vendor/run.sh -- the ecosystem gate.
#
# Runs each targeted real-world PHP project's OWN suite under php and under phl and
# requires the two to answer the same thing. What "the same thing" means is a
# committed BASELINE: the normalized php-vs-phl diff for every step lives in
# projects/<name>/expected/<step>.diff, so an accepted divergence is a file in git
# and a NEW one fails the gate.
#
# The library sources are not committed. Each project is a pinned git ref plus a
# committed composer.lock, installed into build/tests-vendor/, and every source
# change to what comes out of that install is a .patch file in projects/<name>/patches.
#
# Patches may ONLY remove a project's use of a surface PLAN.md §10 refuses because
# php merely DEPRECATES it. That is the whole accepted patch strategy: it lets the
# policy stand and still lets a suite run to the end. Every patch declares its reason
# in a header this script validates, and a patch that CUTS coverage rather than
# rewriting a spelling is counted and printed on every run so it cannot go quiet.
#
# Off the Makefile and off CI on purpose (it needs the network and a real composer).
#
# usage:
#   tests/vendor/run.sh [options] [project ...]
#     --list          list the projects and exit
#     --prepare       fetch + install + patch only, do not run the suites
#     --run-only      skip fetch/install/patch, run the suites against what is there
#     --reinstall     wipe the checkout first
#     --accept        write the current diffs as the new baselines (review them!)
#     --keep-going    do not stop at the first project that fails
#
# env: PHL_BIN PHP_BIN COMPOSER_BIN PHL_VENDOR_WORK

set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
HERE="$ROOT/tests/vendor"
PROJECTS_DIR="$HERE/projects"
WORK="${PHL_VENDOR_WORK:-$ROOT/build/tests-vendor}"

TARGET=$("$ROOT/build-aux/get_target.sh")
PHL_BIN="${PHL_BIN:-$ROOT/build/$TARGET/full/phl}"
PHP_BIN="${PHP_BIN:-php}"
COMPOSER_BIN="${COMPOSER_BIN:-composer}"
# PREPARE and the steps both need to name an engine explicitly.
export PHP_BIN PHL_BIN

# Every reason a patch is allowed to give. One per surface PLAN.md §10 removes or
# rejects because php only deprecates it. Nothing else is a legal patch: an engine
# defect gets FIXED, and a scope cut that is not a deprecation (the timezone
# database, mb_*'s encodings, fileinfo's magic database) is measured, not patched.
ALLOWED_REASONS="
implicit-nullable-param
null-to-non-nullable-param
ord-multibyte
chr-out-of-range
trigger-error-user-error
dollar-brace-interpolation
backtick-operator
strftime
zip-procedural
assert-options
e-strict
session-deprecated-directive
session-save-handler-callables
dateinterval-write
lossy-float-to-int
false-to-array
dynamic-property
non-numeric-increment
base-convert-invalid
"

die() { echo "run.sh: $*" >&2; exit 1; }
note() { echo "== $*"; }

# A gate holds build/.gate-lock and this suite is heavy enough to matter beside it.
if [ -f "$ROOT/build/.gate-lock" ]; then
	held=$(cat "$ROOT/build/.gate-lock" 2>/dev/null || true)
	if [ -n "$held" ] && kill -0 "$held" 2>/dev/null; then
		die "a gate is running (pid $held); this suite would compete with it for the box"
	fi
fi

DO_FETCH=1; DO_RUN=1; REINSTALL=0; ACCEPT=0; KEEP_GOING=0; LIST=0
WANTED=""
while [ $# -gt 0 ]; do
	case "$1" in
		--list) LIST=1 ;;
		--prepare) DO_RUN=0 ;;
		--run-only) DO_FETCH=0 ;;
		--reinstall) REINSTALL=1 ;;
		--accept) ACCEPT=1 ;;
		--keep-going) KEEP_GOING=1 ;;
		-h|--help) sed -n '5,40p' "$0"; exit 0 ;;
		-*) die "unknown option $1" ;;
		*) WANTED="$WANTED $1" ;;
	esac
	shift
done

all_projects() {
	for d in "$PROJECTS_DIR"/*/; do
		[ -f "$d/manifest" ] || continue
		basename "$d"
	done
}

if [ "$LIST" = 1 ]; then
	for p in $(all_projects); do
		# shellcheck disable=SC1090
		( NAME=""; REF=""; REPO=""; . "$PROJECTS_DIR/$p/manifest"
		  printf '%-20s %s @ %s\n' "$p" "$REPO" "$REF" )
	done
	exit 0
fi

[ -n "$WANTED" ] || WANTED=$(all_projects)
[ -n "$WANTED" ] || die "no projects in $PROJECTS_DIR"

[ -x "$PHL_BIN" ] || die "no phl binary at $PHL_BIN (make, or set PHL_BIN)"
command -v "$PHP_BIN" >/dev/null 2>&1 || die "no php oracle ($PHP_BIN)"

# error_reporting is pinned to E_ALL on both engines, as a NUMBER. php parses a constant
# name in an ini value and PHL does not (`-d error_reporting=E_ALL` sets 0 here, which is
# an open row), and the number itself is a php version's business -- so ask each engine
# for its own E_ALL and refuse to measure anything if the two disagree.
ERALL=$("$PHP_BIN" -r 'echo E_ALL;' 2>/dev/null)
ERALL_PHL=$("$PHL_BIN" -r 'echo E_ALL;' 2>/dev/null)
[ -n "$ERALL" ] && [ "$ERALL" = "$ERALL_PHL" ] \
	|| die "E_ALL differs between the engines (php=$ERALL phl=$ERALL_PHL); every
    diagnostic in every step would diverge for that alone. Fix the engine or pin
    PHP_BIN to a php whose E_ALL matches."


# Composer may be a PATH binary or a phar; both are fine.
composer_run() {
	case "$COMPOSER_BIN" in
		*.phar) "$PHP_BIN" "$COMPOSER_BIN" "$@" ;;
		*) "$COMPOSER_BIN" "$@" ;;
	esac
}

# ---------------------------------------------------------------- patch policy

# A patch must carry these three headers before its diff:
#   PHL-Patch-Reason: <one of ALLOWED_REASONS>
#   PHL-Patch-Kind:   rewrite | adapt | cut
#   PHL-Patch-Note:   one line, what it does and what it costs
# `rewrite` changes NO behaviour under php -- `T $x = null` -> `?T $x = null`,
# `chr($n)` -> `chr($n % 256)`, `E_ALL|E_STRICT` -> `E_ALL`. `adapt` still does the
# thing, spelled without the deprecated construct, but is not byte-equivalent under
# php (an E_USER_ERROR the test only needs to be an error becomes E_USER_WARNING).
# `cut` means the project's own coverage SHRINKS -- a test that asks for the
# deprecated answer by name. All three are legal; all three are counted and printed,
# because a `cut` that nobody re-reads is how a policy stops being measured.
validate_patch() {
	f="$1"
	reason=$(sed -n 's/^PHL-Patch-Reason:[[:space:]]*//p' "$f" | head -1)
	kind=$(sed -n 's/^PHL-Patch-Kind:[[:space:]]*//p' "$f" | head -1)
	note=$(sed -n 's/^PHL-Patch-Note:[[:space:]]*//p' "$f" | head -1)
	[ -n "$reason" ] || die "$f: no PHL-Patch-Reason header"
	[ -n "$note" ] || die "$f: no PHL-Patch-Note header"
	case "$kind" in
		rewrite|adapt|cut) : ;;
		*) die "$f: PHL-Patch-Kind must be 'rewrite', 'adapt' or 'cut', got '$kind'" ;;
	esac
	ok=0
	for r in $ALLOWED_REASONS; do
		[ "$r" = "$reason" ] && ok=1
	done
	[ "$ok" = 1 ] || die "$f: '$reason' is not a §10 deprecation surface.
    A patch may only take a project OFF a surface php deprecates and PHL refuses.
    An engine defect is fixed in the engine; a non-deprecation scope cut is measured.
    Allowed:$(echo "$ALLOWED_REASONS" | tr '\n' ' ')"
	echo "$kind"
}

# --------------------------------------------------------------- normalization

# Everything that differs between two runs of the SAME engine, or between two
# engines for reasons no test is about: colour, absolute paths, wall-clock, memory,
# the interpreter's own banner, PHPUnit's random seed and its per-run cache dir.
normalize() {
	# PHPUnit prints a data-set name with the leading slash EATEN, so the checkout
	# path has to be matched in both spellings or a test name looks like a divergence.
	sed -e 's/\x1b\[[0-9;]*[A-Za-z]//g' \
	    -e "s#$CHECKOUT#@ROOT@#g" \
	    -e "s#${CHECKOUT#/}#@ROOT@#g" \
	    -e "s#$PHL_BIN#@ENGINE@#g" \
	    -e "s#$(command -v "$PHP_BIN")#@ENGINE@#g" \
	    -e 's#/home/[^ :]*#@PATH@#g' \
	    -e 's/[0-9]\+\.[0-9]\+ *\(secs\|seconds\|s\b\)/@TIME@/g' \
	    -e 's/^Time: .*/Time: @TIME@/' \
	    -e 's/^ *Duration: .*/  Duration: @TIME@/' \
	    -e 's/Memory: [0-9.]* *[KMG]B/Memory: @MEM@/g' \
	    -e 's/^Runtime: .*/Runtime: @RUNTIME@/' \
	    -e 's/^PHP [0-9][^ ]* /PHP @VERSION@ /' \
	    -e 's/Random Seed:.*/Random Seed: @SEED@/' \
	    -e 's/[0-9]\{1,\}:[0-9][0-9]\.[0-9]\{3\}/@TIME@/g' \
	    -e 's/[[:space:]]*$//' \
	| if [ -n "$NORMALIZE" ]; then sed -e "$NORMALIZE"; else cat; fi
}

# ------------------------------------------------------------------- per project

FAILED=""
for P in $WANTED; do
	PDIR="$PROJECTS_DIR/$P"
	[ -f "$PDIR/manifest" ] || die "no such project: $P"

	# NORMALIZE is an extra sed expression a project may declare for output that is
	# genuinely per-run and about nothing (twig stamps a random hash into every
	# generated template class name). It is per project on purpose: a general
	# hex-eater in normalize() would hide real differences everywhere else.
	NAME=""; REPO=""; REF=""; COMPOSER_FLAGS=""; PRE_INSTALL=""; PREPARE=""; NORMALIZE=""
	STEP_TIMEOUT=""
	# shellcheck disable=SC1090
	. "$PDIR/manifest"
	[ -n "$REPO" ] && [ -n "$REF" ] || die "$P: manifest needs REPO and REF"

	CHECKOUT="$WORK/$P"
	OUTDIR="$WORK/$P.out"
	note "$P  ($REPO @ $REF)"

	if [ "$REINSTALL" = 1 ]; then rm -rf "$CHECKOUT"; fi

	if [ "$DO_FETCH" = 1 ]; then
		mkdir -p "$WORK"
		if [ ! -d "$CHECKOUT/.git" ]; then
			note "$P: clone"
			git clone --quiet --filter=blob:none "$REPO" "$CHECKOUT"
		fi
		git -C "$CHECKOUT" fetch --quiet --tags origin || true
		git -C "$CHECKOUT" -c advice.detachedHead=false checkout --quiet --force "$REF"
		# Undo the last run's patches; they are re-applied below.
		git -C "$CHECKOUT" reset --quiet --hard "$REF"
		git -C "$CHECKOUT" clean --quiet -fd -e vendor

		# PRE_INSTALL runs in the checkout before composer, for REPRODUCIBILITY only --
		# monolog's composer.json sets `config.lock: false`, so composer refuses to write
		# a lock and re-resolves from packagist on every run. It may not touch source.
		if [ -n "$PRE_INSTALL" ]; then
			( cd "$CHECKOUT" && eval "$PRE_INSTALL" ) >>"$WORK/$P.install.log" 2>&1
		fi

		# The lock is OURS: a project whose upstream ships no lock, or ships one that
		# moves, would otherwise install a different tree on every run.
		if [ -f "$PDIR/composer.lock" ]; then
			cp "$PDIR/composer.lock" "$CHECKOUT/composer.lock"
		fi
		note "$P: composer install"
		# COMPOSER_FLAGS is per project and must be justified in its manifest: the only
		# legitimate use is a platform requirement the ORACLE does not satisfy either.
		( cd "$CHECKOUT" && composer_run install --no-interaction --no-progress --no-ansi $COMPOSER_FLAGS ) >"$WORK/$P.install.log" 2>&1 \
			|| { tail -20 "$WORK/$P.install.log"; die "$P: composer install failed (see $WORK/$P.install.log)"; }
		if [ ! -f "$PDIR/composer.lock" ] && [ -f "$CHECKOUT/composer.lock" ]; then
			cp "$CHECKOUT/composer.lock" "$PDIR/composer.lock"
			note "$P: no committed lock -- wrote $PDIR/composer.lock, COMMIT IT"
		fi

		# Patches go on after the install, because most of them are on vendor code.
		CUTS=0; REWRITES=0; ADAPTS=0
		if [ -d "$PDIR/patches" ]; then
			for f in "$PDIR"/patches/*.patch; do
				[ -f "$f" ] || continue
				kind=$(validate_patch "$f")
				case "$kind" in
					cut) CUTS=$((CUTS + 1)) ;;
					adapt) ADAPTS=$((ADAPTS + 1)) ;;
					*) REWRITES=$((REWRITES + 1)) ;;
				esac
				# Idempotent on purpose: the checkout is reset between runs but
				# vendor/ is KEPT (a re-install every run would cost minutes and a
				# network), so most patches are already in place from last time.
				if ( cd "$CHECKOUT" && git apply --check --reverse --whitespace=nowarn "$f" ) 2>/dev/null; then
					: # already applied
				elif ( cd "$CHECKOUT" && git apply --whitespace=nowarn "$f" ); then
					: # applied now
				else
					die "$P: $(basename "$f") does not apply -- and is not already applied.
    The pinned tree moved under it, or the patch is stale. Regenerate it against a
    fresh install (tests/vendor/run.sh --reinstall --prepare $P)."
				fi
			done
		fi
		note "$P: patches applied -- $REWRITES rewrite, $ADAPTS adapt, $CUTS cut"
		if [ -n "$PREPARE" ]; then
			( cd "$CHECKOUT" && eval "$PREPARE" ) >>"$WORK/$P.install.log" 2>&1
		fi
	fi

	[ "$DO_RUN" = 1 ] || continue
	[ -d "$CHECKOUT" ] || die "$P: nothing installed at $CHECKOUT (drop --run-only)"

	mkdir -p "$OUTDIR" "$PDIR/expected"
	STEP_N=0
	PROJECT_BAD=0
	# steps: one shell command per line, run from the checkout root. Blank lines and
	# # comments ignored. The name in the first field is the step's id, then a TAB,
	# then the command -- which names the engine as $PHL_VENDOR_ENGINE, UNQUOTED,
	# because it is a command with its ini flags and not a bare path.
	while IFS= read -r line; do
		case "$line" in ''|\#*) continue ;; esac
		STEP_ID=${line%%	*}
		STEP_CMD=${line#*	}
		[ "$STEP_ID" = "$STEP_CMD" ] && die "$P: steps line needs <id><TAB><command>: $line"
		STEP_N=$((STEP_N + 1))

		for eng in php phl; do
			# error_reporting is pinned on BOTH sides. This box's php.ini masks
			# E_DEPRECATED (22527) where PHL starts at E_ALL (30719), so an unpinned
			# run reports php-correct deprecations as PHL divergences by the hundred.
			# XDEBUG_MODE=off for the same reason: xdebug changes what php prints.
			case $eng in
				php) BIN="$PHP_BIN -d error_reporting=$ERALL"; ENV="XDEBUG_MODE=off" ;;
				phl) BIN="$PHL_BIN -d error_reporting=$ERALL"; ENV="" ;;
			esac
			# Both engines must start from the SAME tree. A suite leaves state behind --
			# PHPUnit writes .phpunit.result.cache, twig writes compiled templates -- and
			# without this the second engine reads what the first one wrote, and the run
			# after that reads both. (That is not hypothetical: it is how §3 N's crash
			# hid, and it would make every measurement here depend on run order.)
			# vendor/ and our lock survive; everything else untracked or ignored goes.
			git -C "$CHECKOUT" clean -qfdx -e vendor -e composer.lock
			set +e
			( cd "$CHECKOUT"
			  ulimit -v "${PHL_VENDOR_VMAX:-8000000}" 2>/dev/null || true
			  # stdin from /dev/null: a step must never wait for input, and a tool
			  # that finds a readable stdin may READ it instead of its own config
			  # (phpcs with no arguments lints STDIN and reports one empty file).
			  # The SAME wall-clock budget for both engines, so a timeout is a
			  # comparable answer and not an artefact of which one ran first. A step
			  # with no budget can hang the gate for ever: phpcs over 711 files is
			  # 9s under php and minutes under phl, and a gate nobody can finish is
			  # a gate nobody runs.
			  env $ENV PHL_VENDOR_ENGINE="$BIN" timeout "${STEP_TIMEOUT:-${PHL_VENDOR_TIMEOUT:-600}}" \
				sh -c "$STEP_CMD" </dev/null ) >"$OUTDIR/$STEP_ID.$eng" 2>&1
			st=$?
			set -e
			# The exit STATUS is part of the answer: a suite that fails the same rows
			# under both engines still has to fail the same way to the shell that ran it.
			echo "@exit@ $st" >>"$OUTDIR/$STEP_ID.$eng"
		done

		normalize <"$OUTDIR/$STEP_ID.php" >"$OUTDIR/$STEP_ID.php.norm"
		normalize <"$OUTDIR/$STEP_ID.phl" >"$OUTDIR/$STEP_ID.phl.norm"
		# Stable labels: the default header carries mtimes and absolute paths, which
		# would make every run's diff differ from the committed one for no reason.
		diff -u --label php --label phl \
			"$OUTDIR/$STEP_ID.php.norm" "$OUTDIR/$STEP_ID.phl.norm" >"$OUTDIR/$STEP_ID.diff" || true

		# A CRASH is never an accepted answer. A signal death (128+) under the engine
		# under test fails the gate even when the diff matches the baseline, because
		# "we already knew it segfaults" is exactly how a crash stops being work.
		CRASHED=$(sed -n 's/^@exit@ //p' "$OUTDIR/$STEP_ID.phl" | tail -1)
		# 128+N is a signal death; 255 is php's own exit for an uncaught fatal, which
		# is an ordinary answer and belongs in the diff like any other.
		if [ -n "$CRASHED" ] && [ "$CRASHED" -ge 128 ] && [ "$CRASHED" -le 192 ] 2>/dev/null; then
			printf '   %-22s CRASHED (exit %s) -- %s\n' "$STEP_ID" "$CRASHED" "$OUTDIR/$STEP_ID.phl"
			tail -3 "$OUTDIR/$STEP_ID.phl" | sed 's/^/      /'
			PROJECT_BAD=1
			continue
		fi

		BASE="$PDIR/expected/$STEP_ID.diff"
		if [ "$ACCEPT" = 1 ]; then
			cp "$OUTDIR/$STEP_ID.diff" "$BASE"
			printf '   %-22s ACCEPTED  (%s diff lines)\n' "$STEP_ID" "$(wc -l <"$BASE" | tr -d ' ')"
			continue
		fi
		[ -f "$BASE" ] || : >"$BASE"
		if diff -q "$BASE" "$OUTDIR/$STEP_ID.diff" >/dev/null 2>&1; then
			if [ -s "$BASE" ]; then
				printf '   %-22s PARITY (known: %s diff lines)\n' "$STEP_ID" "$(wc -l <"$BASE" | tr -d ' ')"
			else
				printf '   %-22s PARITY\n' "$STEP_ID"
			fi
		else
			printf '   %-22s DIVERGED  -- %s\n' "$STEP_ID" "$OUTDIR/$STEP_ID.diff"
			diff -u "$BASE" "$OUTDIR/$STEP_ID.diff" | sed -n '3,25p' | sed 's/^/      /'
			PROJECT_BAD=1
		fi
	done <"$PDIR/steps"

	[ "$STEP_N" -gt 0 ] || die "$P: steps file ran nothing"
	if [ "$PROJECT_BAD" = 1 ]; then
		FAILED="$FAILED $P"
		[ "$KEEP_GOING" = 1 ] || break
	fi
done

if [ -n "$FAILED" ]; then
	echo
	echo "ECOSYSTEM GATE: FAILED --$FAILED"
	echo "A step diverged from its committed baseline. Either the engine regressed, or"
	echo "the answer changed on purpose -- in which case review the diff and --accept it."
	exit 1
fi
if [ "$DO_RUN" = 1 ]; then
	echo
	echo "ECOSYSTEM GATE: ok"
fi
exit 0
