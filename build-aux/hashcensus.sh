#!/bin/sh
# PERF.md §7 -- the name-lookup census, run end to end.
#
# Answers "which CALL SITE is spending the engine's name hashing, and on how
# many bytes". The sampler says how large SyHashGet's subsystem is; it cannot
# say which of its 273 call sites that is, because attribution costs a frame
# per sample and a leaf-plus-caller table only reaches the loudest few. This
# counts every lookup, exactly, and a count does not care that this box is
# loaded (§7).
#
# The instrument is in src/sx/sxds.c behind -DPHL_HASH_CENSUS; this script
# builds it into its own target, runs the workload ONCE (unlike the heap
# census, nothing here needs to know a peak in advance), and resolves every
# site through addr2line.
#
# Usage: build-aux/hashcensus.sh [-b] <script.php> [args...]
#   -b   rebuild the census binary first (needed after any src/ change)
# Env:
#   PHL_HCENSUS_TOP  rows in the ranked table (default 30)
#   PHL_HCENSUS_KEEP keep the raw dump at this path instead of a temp file
#   PHL_FORCE=1      run even while a gate holds the build lock
#
# The workload runs in the directory you invoked this FROM, not in the repo --
# a real one (phpcs, phpstan) reads its configuration out of its own project.
ROOT=$(cd "$(dirname "$0")/.." && pwd) || exit 1
HERE=$PWD
cd "$ROOT" || exit 1

if [ -x build-aux/mk.sh ]; then MAKE=build-aux/mk.sh; else MAKE=${MAKE:-make}; fi

TARGET=x86_64-linux-gnu-hcensus
BIN="$ROOT/build/$TARGET/full/phl"
TOP=${PHL_HCENSUS_TOP:-30}

if [ -z "$PHL_FORCE" ] && [ -f build/.gate-lock ]; then
	pid=$(cat build/.gate-lock 2>/dev/null)
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		echo "hashcensus.sh: a gate is running (build-aux/gates.sh, pid $pid) -- refusing." >&2
		exit 1
	fi
	rm -f build/.gate-lock
fi

BUILD=0
if [ "$1" = "-b" ]; then BUILD=1; shift; fi
if [ ! -x "$BIN" ]; then BUILD=1; fi
if [ $# -lt 1 ]; then
	echo "usage: build-aux/hashcensus.sh [-b] <script.php> [args...]" >&2
	exit 2
fi

if [ $BUILD = 1 ]; then
	echo "hashcensus: building $BIN ..." >&2
	# -O2 rather than -O3, like the heap census: the answer is WHICH SITE, and
	# -O2 keeps a return address attributable to the function that asked.
	PHL_FORCE=1 $MAKE TARGET="$TARGET" MODE=full -j"$(nproc 2>/dev/null || echo 4)" \
		build full_OPT_CFLAGS="-O2 -g -DPHL_HASH_CENSUS" >/dev/null || exit 1
fi

DUMP=${PHL_HCENSUS_KEEP:-$(mktemp)}
trap 'rm -f "$DUMP.addr" "$DUMP.name"; if [ -z "$PHL_HCENSUS_KEEP" ]; then rm -f "$DUMP"; fi' EXIT INT TERM

cd "$HERE" || exit 1
PHL_HCENSUS_OUT="$DUMP" "$BIN" "$@" >/dev/null 2>&1
if [ ! -s "$DUMP" ]; then
	echo "hashcensus: the workload wrote no dump -- did it run?" >&2
	exit 1
fi

# One addr2line for every distinct site: -f -C and NOT -i, so each address
# answers in exactly two lines and the pairing below cannot slip.
SITES=$(awk '/^SITE /{print $2}' "$DUMP" | sort -u)
NAMES=$(printf '%s\n' $SITES | xargs addr2line -e "$BIN" -f -C 2>/dev/null)
printf '%s\n' "$SITES" > "$DUMP.addr"
printf '%s\n' "$NAMES" > "$DUMP.name"

LC_ALL=C awk -v top="$TOP" '
	FILENAME == ARGV[1] { addr[++na] = $0; next }
	FILENAME == ARGV[2] {
		if (++nl % 2 == 1) { fn[(nl+1)/2] = $0 }
		else { loc[nl/2] = $0 }
		next
	}
	/^# lookups/ { tot = $3; totB = $5; totH = $7; if (/TRUNCATED/) trunc = 1; next }
	/^SITE / {
		a = $2
		ord[++ns] = a; call[a] = $3; byte[a] = $4; hit[a] = $5; ci[a] = $6
	}
	function rank(o, v, n,   i, j, t) {
		for (i = 1; i <= n; i++) for (j = i+1; j <= n; j++)
			if (v[o[j]] > v[o[i]]) { t=o[i]; o[i]=o[j]; o[j]=t }
	}
	END {
		for (i = 1; i <= na; i++) { name[addr[i]] = fn[i]; where[addr[i]] = loc[i] }
		printf "\n  %s lookups, %.2f GB of key bytes hashed, %.1f%% found something",
			tot, totB/1e9, 100*totH/tot
		if (trunc) printf "   *** TRUNCATED -- the site table filled ***"
		printf "\n\n"
		printf "  %6s %13s %12s %6s %3s  %s\n", "share", "lookups", "key bytes", "hit", "ci", "site"
		rank(ord, call, ns)
		for (i = 1; i <= ns && i <= top; i++) {
			a = ord[i]
			printf "  %5.2f%% %13s %12s %5.1f%% %3s  %s\n",
				100*call[a]/tot, call[a], byte[a],
				100*hit[a]/call[a], (ci[a] > 0 ? "yes" : "-"),
				(a in name ? name[a] "  " where[a] : a)
		}
		printf "\n  ci = the table folds case (SyStrHash/SyStrnmicmp) rather than comparing bytes.\n"
		printf "  A site with a low hit rate and a large key is the engine hashing a long name\n"
		printf "  to learn that it is not there -- see PERF.md P4.\n\n"
	}
' "$DUMP.addr" "$DUMP.name" "$DUMP"
