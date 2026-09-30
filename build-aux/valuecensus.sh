#!/bin/sh
# PERF.md §7 -- the value-primitive census, run end to end.
#
# Answers "which CALL SITE spends the engine's value primitives, and how many of
# its calls had anything to DO". PH7_MemObjRelease is the most-called function in
# the engine -- 2.72 billion calls on the phpcs step of record -- and it appears in
# NEITHER of the other two censuses, because it allocates nothing and looks nothing
# up. That is exactly why PERF.md P10 had no heading for three sessions.
#
# The instrument is in src/ph7/memobj.c behind -DPHL_VALUE_CENSUS (the doors it
# counts are in src/ph7/ph7int.h); this script builds it into its own target, runs
# the workload ONCE, and resolves every site through addr2line.
#
# Usage: build-aux/valuecensus.sh [-b] [-c] <script.php> [args...]
#   -b   rebuild the census binary first (needed after any src/ change)
#   -c   attribute each row to the CALLER of the function that made the call,
#        rather than to the line that made it. Its own target and its own binary.
#
# Read the `work` column first. A site with millions of calls and a work rate near
# zero is the engine initialising, pushing and tearing down slots that never held
# anything -- which is P10 item 1, and the reason this instrument exists.
#
# Env:
#   PHL_VCENSUS_TOP   rows per primitive (default 15)
#   PHL_VCENSUS_KEEP  keep the raw dump at this path instead of a temp file
#   PHL_FORCE=1       run even while a gate holds the build lock
#
# The workload runs in the directory you invoked this FROM, not in the repo.
ROOT=$(cd "$(dirname "$0")/.." && pwd) || exit 1
HERE=$PWD
cd "$ROOT" || exit 1

if [ -x build-aux/mk.sh ]; then MAKE=build-aux/mk.sh; else MAKE=${MAKE:-make}; fi

TARGET=x86_64-linux-gnu-vcensus
CALLER_CFLAGS=
BIN="$ROOT/build/$TARGET/full/phl"
TOP=${PHL_VCENSUS_TOP:-15}

if [ -z "$PHL_FORCE" ] && [ -f build/.gate-lock ]; then
	pid=$(cat build/.gate-lock 2>/dev/null)
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		echo "valuecensus.sh: a gate is running (build-aux/gates.sh, pid $pid) -- refusing." >&2
		exit 1
	fi
	rm -f build/.gate-lock
fi

BUILD=0
while :; do
	case $1 in
	-b) BUILD=1; shift ;;
	-c)
		TARGET=x86_64-linux-gnu-vcaller
		CALLER_CFLAGS=" -fno-omit-frame-pointer -Wno-frame-address -DPHL_VCENSUS_CALLER"
		BIN="$ROOT/build/$TARGET/full/phl"
		shift ;;
	*) break ;;
	esac
done
if [ ! -x "$BIN" ]; then BUILD=1; fi
if [ $# -lt 1 ]; then
	echo "usage: build-aux/valuecensus.sh [-b] [-c] <script.php> [args...]" >&2
	exit 2
fi

if [ $BUILD = 1 ]; then
	echo "valuecensus: building $BIN ..." >&2
	# -O2 rather than -O3, like the other two censuses: the answer is WHICH SITE,
	# and -O2 keeps a return address attributable to the line that asked.
	PHL_FORCE=1 $MAKE TARGET="$TARGET" MODE=full -j"$(nproc 2>/dev/null || echo 4)" \
		build full_OPT_CFLAGS="-O2 -g -DPHL_VALUE_CENSUS$CALLER_CFLAGS" >/dev/null || exit 1
fi

DUMP=${PHL_VCENSUS_KEEP:-$(mktemp)}
trap 'rm -f "$DUMP.addr" "$DUMP.name"; if [ -z "$PHL_VCENSUS_KEEP" ]; then rm -f "$DUMP"; fi' EXIT INT TERM

cd "$HERE" || exit 1
PHL_VCENSUS_OUT="$DUMP" "$BIN" "$@" >/dev/null 2>&1
if [ ! -s "$DUMP" ]; then
	echo "valuecensus: the workload wrote no dump -- did it run?" >&2
	exit 1
fi

SITES=$(awk '/^SITE /{print $2}' "$DUMP" | sort -u)
NAMES=$(printf '%s\n' $SITES | xargs addr2line -e "$BIN" -f -C 2>/dev/null)
printf '%s\n' "$SITES" > "$DUMP.addr"
printf '%s\n' "$NAMES" > "$DUMP.name"

LC_ALL=C awk -v top="$TOP" -v caller="${CALLER_CFLAGS:+1}" '
	FILENAME == ARGV[1] { addr[++na] = $0; next }
	FILENAME == ARGV[2] {
		if (++nl % 2 == 1) { fn[(nl+1)/2] = $0 }
		else { loc[nl/2] = $0 }
		next
	}
	/^# kind/ { ktot[$3] = $4; kwork[$3] = $5; if (/TRUNCATED/) trunc = 1; next }
	/^SITE / {
		k = $3; key = $2 SUBSEP k
		ord[k, ++ns[k]] = key; site[key] = $2; call[key] = $4; work[key] = $5
	}
	function rank(k, n,   i, j, t) {
		for (i = 1; i <= n; i++) for (j = i+1; j <= n; j++)
			if (call[ord[k, j]] > call[ord[k, i]]) { t=ord[k,i]; ord[k,i]=ord[k,j]; ord[k,j]=t }
	}
	END {
		kn[0] = "PH7_MemObjRelease"; kn[1] = "PH7_MemObjLoad"
		kn[2] = "PH7_MemObjStore";   kn[3] = "PH7_MemObjInit"
		for (i = 1; i <= na; i++) { name[addr[i]] = fn[i]; where[addr[i]] = loc[i] }
		tot = 0; for (k = 0; k <= 3; k++) tot += ktot[k]
		printf "\n  %s calls into the four value primitives", tot
		if (trunc) printf "   *** TRUNCATED -- the site table filled ***"
		printf "\n"
		for (k = 0; k <= 3; k++) {
			if (ktot[k] + 0 == 0) continue
			printf "\n  %s -- %s calls, %.1f%% of them had work to do\n",
				kn[k], ktot[k], 100*kwork[k]/ktot[k]
			printf "  %6s %14s %6s  %s\n", "share", "calls", "work", "site"
			rank(k, ns[k])
			for (i = 1; i <= ns[k] && i <= top; i++) {
				key = ord[k, i]; a = site[key]
				printf "  %5.2f%% %14s %5.1f%%  %s\n",
					100*call[key]/ktot[k], call[key], 100*work[key]/call[key],
					(a in name ? name[a] "  " where[a] : a)
			}
		}
		printf "\n  work = the call had something to do: a release that owned something, a\n"
		printf "  load/store that took a container reference. A large site at a low work\n"
		printf "  rate is a slot that did not need to exist -- PERF.md P10 item 1.\n"
		if (caller) printf "  Rows name the CALLER of the function that called (-c), not the line.\n"
		printf "\n"
	}
' "$DUMP.addr" "$DUMP.name" "$DUMP"
