#!/bin/sh
# The heap census, run end to end.
#
# Answers "where are the bytes at the high-water mark, and who asked for them".
# Five sessions hand-rolled this before it lived in the tree; the instrument
# itself is in src/sx/sxmem.c behind -DPHL_MEM_CENSUS.
#
# The protocol is two runs of the SAME workload, because the peak is only known
# once it is past: run one learns it, run two dumps the table when the live
# bytes reach it again. So the workload must be DETERMINISTIC, and a real tool
# often is not: phpcs writes a .phpcs.cache and the second run measures the
# cache instead of the engine. This script compares the two peaks and says so
# when they disagree; clear the tool's cache (or pass its --no-cache) first.
#
# Usage: build-aux/census.sh [-b] <script.php> [args...]
#   -b   rebuild the census binary first (needed after any src/ change)
# Env:
#   PHL_CENSUS_SLOTS  records the table can hold; raise it if a run says
#                     TRUNCATED (default 2M, which is ~80 MB and holds a
#                     1.5M-object heap)
#   PHL_CENSUS_TOP    rows in the ranked table (default 30)
#   PHL_CENSUS_KEEP   keep the raw dump at this path instead of a temp file
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

TARGET=x86_64-linux-gnu-census
BIN="$ROOT/build/$TARGET/full/phl"
TOP=${PHL_CENSUS_TOP:-30}

if [ -z "$PHL_FORCE" ] && [ -f build/.gate-lock ]; then
	pid=$(cat build/.gate-lock 2>/dev/null)
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		echo "census.sh: a gate is running (build-aux/gates.sh, pid $pid) -- refusing." >&2
		exit 1
	fi
	rm -f build/.gate-lock
fi

BUILD=0
if [ "$1" = "-b" ]; then BUILD=1; shift; fi
if [ ! -x "$BIN" ]; then BUILD=1; fi
if [ $# -lt 1 ]; then
	echo "usage: build-aux/census.sh [-b] <script.php> [args...]" >&2
	exit 2
fi

if [ $BUILD = 1 ]; then
	echo "census: building $BIN ..." >&2
	# -O2 rather than -O3: the census is about WHERE the bytes are, and -O2
	# keeps the return addresses attributable to the function that asked.
	PHL_FORCE=1 $MAKE TARGET="$TARGET" MODE=full -j"$(nproc 2>/dev/null || echo 4)" \
		build full_OPT_CFLAGS="-O2 -g -DPHL_MEM_CENSUS" >/dev/null || exit 1
fi

DUMP=${PHL_CENSUS_KEEP:-$(mktemp)}
trap 'rm -f "$DUMP.addr" "$DUMP.name"; if [ -z "$PHL_CENSUS_KEEP" ]; then rm -f "$DUMP"; fi' EXIT INT TERM

cd "$HERE" || exit 1
echo "census: run 1 of 2 (learning the peak) ..." >&2
PEAK=$(PHL_CENSUS_OUT=/dev/null "$BIN" "$@" 2>&1 >/dev/null \
	| sed -n 's/^census: peak recorded live bytes \([0-9]*\) .*/\1/p' | tail -1)
if [ -z "$PEAK" ] || [ "$PEAK" = 0 ]; then
	echo "census: run 1 reported no peak -- did the workload run?" >&2
	exit 1
fi

echo "census: run 2 of 2 (dumping at $PEAK bytes) ..." >&2
PEAK2=$(PHL_CENSUS_AT="$PEAK" PHL_CENSUS_OUT="$DUMP" "$BIN" "$@" 2>&1 >/dev/null \
	| sed -n 's/^census: peak recorded live bytes \([0-9]*\) .*/\1/p' | tail -1)
if [ ! -s "$DUMP" ]; then
	echo "census: run 2 never reached $PEAK bytes (it peaked at ${PEAK2:-0})." >&2
	echo "        The workload is not deterministic -- clear its cache and retry." >&2
	exit 1
fi
if [ -n "$PEAK2" ] && [ "$PEAK2" -gt 0 ]; then
	# Within 2%: the same workload twice. Outside it, run two allocated a
	# different heap and the table below describes a moment that is not the
	# peak it claims to be.
	if [ $(( (PEAK2 - PEAK) * 100 / PEAK )) -gt 2 ] || [ $(( (PEAK - PEAK2) * 100 / PEAK )) -gt 2 ]; then
		echo "census: WARNING run 1 peaked at $PEAK, run 2 at $PEAK2 -- the two runs" >&2
		echo "        did not do the same work. Clear the workload's cache first." >&2
	fi
fi

# One addr2line for every distinct site: -f -C and NOT -i, so each address
# answers in exactly two lines and the pairing below cannot slip.
SITES=$(awk '/^SITE /{print $2}' "$DUMP" | sort -u)
NAMES=$(printf '%s\n' $SITES | xargs addr2line -e "$BIN" -f -C 2>/dev/null)

printf '%s\n' "$SITES" > "$DUMP.addr"
printf '%s\n' "$NAMES" > "$DUMP.name"

LC_ALL=C awk -v top="$TOP" '
	# Pair each address with its addr2line answer (function, then file:line).
	FILENAME == ARGV[1] { addr[++na] = $0; next }
	FILENAME == ARGV[2] {
		if (++nl % 2 == 1) { fn[(nl+1)/2] = $0 }
		else { loc[nl/2] = $0 }
		next
	}
	/^# live-bytes/ { recs = $5; next }
	/TRUNCATED/     { trunc = 1 }
	/^SITE / {
		a = $2; kind = $3; cnt = $4; chunk = $5; req = $6
		size = $7; lo = $8; hi = $9
		totChunk += chunk; totReq += req
		# Roll up twice: once per SITE, and once per (site, kind, asked-band).
		# The raw dump keys on the exact chunk size as well, which for a direct
		# block is just the request plus a header -- one row per byte of string
		# length, which buries the shape it is meant to show.
		if (!(a in siteChunk)) { siteOrd[++ns] = a }
		siteChunk[a] += chunk; siteCnt[a] += cnt
		k = a SUBSEP kind SUBSEP lo SUBSEP hi
		if (!(k in shChunk)) { shOrd[++nsh] = k; shA[k]=a; shK[k]=kind; shLo[k]=lo; shHi[k]=hi }
		shChunk[k] += chunk; shCnt[k] += cnt; shReq[k] += req
	}
	function label(a) { return (a in name ? name[a] "  " where[a] : a) }
	function rank(ord, val, n,   i, j, t) {
		for (i = 1; i <= n; i++) for (j = i+1; j <= n; j++)
			if (val[ord[j]] > val[ord[i]]) { t=ord[i]; ord[i]=ord[j]; ord[j]=t }
	}
	END {
		for (i = 1; i <= na; i++) { name[addr[i]] = fn[i]; where[addr[i]] = loc[i] }
		printf "\n  peak live %.1f MiB in %s objects", totChunk/1048576, recs
		if (trunc) printf "   *** TRUNCATED -- raise PHL_CENSUS_SLOTS ***"
		printf "\n  of which  %.1f MiB (%.0f%%) is what the allocator spent past what was asked for\n\n",
			(totChunk-totReq)/1048576, 100*(totChunk-totReq)/totChunk

		print "  BY SITE -- whose bytes they are"
		printf "  %9s %6s %9s  %s\n", "bytes", "share", "count", "site"
		rank(siteOrd, siteChunk, ns)
		for (i = 1; i <= ns && i <= top; i++) {
			a = siteOrd[i]
			printf "  %8.2fM %5.1f%% %9d  %s\n", siteChunk[a]/1048576,
				100*siteChunk[a]/totChunk, siteCnt[a], label(a)
		}
		if (ns > top) printf "  %8s %5s %9s  ... and %d more sites\n", "", "", "", ns-top

		print ""
		print "  BY SHAPE -- what one of them cost, against what it asked for"
		printf "  %9s %6s %9s %7s %9s  %s\n", "bytes", "share", "count", "each", "asked", "site"
		rank(shOrd, shChunk, nsh)
		for (i = 1; i <= nsh && i <= top; i++) {
			k = shOrd[i]; a = shA[k]
			printf "  %8.2fM %5.1f%% %9d %7d %4d-%-4d %-6s %s\n", shChunk[k]/1048576,
				100*shChunk[k]/totChunk, shCnt[k], shChunk[k]/shCnt[k],
				shLo[k], shHi[k], shK[k], label(a)
		}
		if (nsh > top) printf "  %8s %5s %9s %7s %9s  ... and %d more rows\n", "", "", "", "", "", nsh-top
		print ""
	}
' "$DUMP.addr" "$DUMP.name" "$DUMP"

rm -f "$DUMP.addr" "$DUMP.name"
if [ -n "$PHL_CENSUS_KEEP" ]; then
	echo "  raw dump: $DUMP" >&2
fi
