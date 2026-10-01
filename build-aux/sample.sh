#!/bin/sh
# The CPU-time sampler, run end to end.
#
# Profiles the ORDINARY release binary (build/<target>/full/phl) by preloading
# build-aux/sampler.c, so what is measured is what ships. `perf` and `gdb -p`
# are both blocked on this box; gprof is for call COUNTS only.
#
# Usage: build-aux/sample.sh [-b] <script.php> [args...]
#   -b   rebuild the release binary first
# Env:
#   PHL_SAMPLE_US     tick in microseconds of CPU time (default 1000 = 1 kHz)
#   PHL_SAMPLE_DEPTH  frames kept per sample, 1..8 (default 4)
#   PHL_SAMPLE_TOP    rows in each table (default 25)
#   PHL_SAMPLE_KEEP   keep the raw dump at this path
#   PHL_FORCE=1       run even while a gate holds the build lock
#
# The workload runs in the directory you invoked this FROM, not in the repo --
# a real one (phpcs, phpstan) reads its configuration out of its own project.
ROOT=$(cd "$(dirname "$0")/.." && pwd) || exit 1
HERE=$PWD
cd "$ROOT" || exit 1

# build-aux/mk.sh is this box's wrapper (it supplies the header-root overrides a
# `full` build needs here); anywhere else plain make is the same command.
if [ -x build-aux/mk.sh ]; then MAKE=build-aux/mk.sh; else MAKE=${MAKE:-make}; fi

TARGET=${TARGET:-$(CC=${CC:-cc} ./build-aux/get_target.sh)}
BIN="$ROOT/build/$TARGET/full/phl"
SO="$ROOT/build/$TARGET/sampler.so"
TOP=${PHL_SAMPLE_TOP:-25}

if [ -z "$PHL_FORCE" ] && [ -f build/.gate-lock ]; then
	pid=$(cat build/.gate-lock 2>/dev/null)
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		echo "sample.sh: a gate is running (build-aux/gates.sh, pid $pid) -- refusing." >&2
		exit 1
	fi
	rm -f build/.gate-lock
fi

if [ "$1" = "-b" ]; then
	shift
	$MAKE MODE=full TARGET="$TARGET" -j"$(nproc 2>/dev/null || echo 4)" build >/dev/null || exit 1
fi
if [ $# -lt 1 ]; then
	echo "usage: build-aux/sample.sh [-b] <script.php> [args...]" >&2
	exit 2
fi
if [ ! -x "$BIN" ]; then
	echo "sample.sh: no $BIN -- run with -b" >&2
	exit 1
fi
if [ ! -f "$SO" ] || [ build-aux/sampler.c -nt "$SO" ]; then
	mkdir -p "$(dirname "$SO")"
	${CC:-cc} -O2 -g -fPIC -shared -W -Wall -Werror -o "$SO" build-aux/sampler.c -ldl || exit 1
fi
cd "$HERE" || exit 1

DUMP=${PHL_SAMPLE_KEEP:-$(mktemp)}
RES=$(mktemp)
trap 'rm -f "$RES" "$RES.pairs"; if [ -z "$PHL_SAMPLE_KEEP" ]; then rm -f "$DUMP"; fi' EXIT INT TERM

PHL_SAMPLE_OUT="$DUMP" LD_PRELOAD="$SO" "$BIN" "$@" >/dev/null || true
if [ ! -s "$DUMP" ]; then
	echo "sample.sh: no samples -- did the workload run long enough?" >&2
	exit 1
fi

# Resolve every distinct pc, one addr2line per module. Anything not in the
# main executable (libc, pcre2, libxml) usually has no symbols worth the call,
# but it still has to be NAMED or its share silently joins someone else's.
awk '/^MODULE /{print "M " $2 " " $3}
     /^STACK /{for(i=3;i<=NF;i++){split($i,a,":"); print "P " a[1] " " a[2]}}' "$DUMP" \
	| sort -u > "$RES.pairs"
: > "$RES"
awk '/^M /{print $2, $3}' "$RES.pairs" | while read -r idx path; do
	addrs=$(awk -v m="$idx" '$1=="P" && $2==m {print $3}' "$RES.pairs")
	[ -z "$addrs" ] && continue
	if [ -r "$path" ]; then
		# shellcheck disable=SC2086
		printf '%s\n' $addrs | xargs addr2line -e "$path" -f -C 2>/dev/null \
			| awk -v m="$idx" -v A="$(printf '%s\n' $addrs | tr '\n' ' ')" '
				BEGIN { n = split(A, addr, " ") }
				{ if (++l % 2 == 1) fn = $0
				  else { i = (l/2); printf "%s %s %s\n", m, addr[i], fn } }' >> "$RES"
	fi
done

LC_ALL=C awk -v top="$TOP" '
	FILENAME == ARGV[1] { sym[$1 SUBSEP $2] = $3; next }
	/^# phl sampler/ { total = $5; dropped = $7; next }
	/^MODULE /       { modname[$2] = $3; next }
	/^STACK / {
		c = $2
		for (i = 3; i <= NF; i++) {
			split($i, a, ":")
			f[i-2] = name(a[1], a[2])
		}
		d = NF - 2
		self[f[1]] += c
		if (d >= 2) { pair[f[1] SUBSEP f[2]] += c }
		# Inclusive: a frame is credited once per sample even if it recurses.
		delete seen
		for (i = 1; i <= d; i++) if (!(f[i] in seen)) { seen[f[i]] = 1; incl[f[i]] += c }
	}
	function name(m, off,   k, b) {
		k = m SUBSEP off
		if (k in sym) return sym[k]
		b = modname[m]
		sub(/.*\//, "", b)
		return b ? b "+0x" off : "0x" off
	}
	function rank(ord, val, n,   i, j, t) {
		for (i = 1; i <= n; i++) for (j = i+1; j <= n; j++)
			if (val[ord[j]] > val[ord[i]]) { t=ord[i]; ord[i]=ord[j]; ord[j]=t }
	}
	END {
		printf "\n  %d samples of CPU time (%d dropped)\n\n", total, dropped
		n = 0; for (k in self) ord[++n] = k
		rank(ord, self, n)
		print "  SELF -- where the cycles are actually spent"
		printf "  %7s %9s  %s\n", "share", "samples", "function"
		for (i = 1; i <= n && i <= top; i++)
			printf "  %6.2f%% %9d  %s\n", 100*self[ord[i]]/total, self[ord[i]], ord[i]
		print ""
		m = 0; for (k in incl) ord2[++m] = k
		rank(ord2, incl, m)
		print "  INCLUSIVE -- and who was on the stack while they were"
		printf "  %7s %9s  %s\n", "share", "samples", "function"
		for (i = 1; i <= m && i <= top; i++)
			printf "  %6.2f%% %9d  %s\n", 100*incl[ord2[i]]/total, incl[ord2[i]], ord2[i]
		print ""
		p = 0; for (k in pair) ord3[++p] = k
		rank(ord3, pair, p)
		print "  CALLERS -- the leaf, and who asked for it"
		printf "  %7s %9s  %s\n", "share", "samples", "leaf  <-  caller"
		for (i = 1; i <= p && i <= top; i++) {
			split(ord3[i], q, SUBSEP)
			printf "  %6.2f%% %9d  %s  <-  %s\n", 100*pair[ord3[i]]/total, pair[ord3[i]], q[1], q[2]
		}
		print ""
	}
' "$RES" "$DUMP"

rm -f "$RES.pairs"
if [ -n "$PHL_SAMPLE_KEEP" ]; then
	echo "  raw dump: $DUMP" >&2
fi
