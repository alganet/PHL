#!/bin/sh
# Triage the corpus's bare-`skip` SKIPIFs (burn-down).
#
# A test whose SKIPIF emits `skip` with NO reason never runs under the php
# oracle and nobody can tell whether it is guarding a real divergence or just
# hiding one. That is not hypothetical: smoke/oo/object_cast_to_array.phpt sat
# behind such a guard asserting PHL's WRONG (array)-cast keys, and only a
# Windows run surfaced it (19 Jul 2026).
#
# MODES
#   build-aux/triage-bare-skips.sh <out-file>
#       Bare-skip triage. For each bare-skip test, strip the SKIPIF and run the
#       test under php:
#         AGREES  -> php produces the expected output; the guard is unnecessary
#                    and the test can go cross-engine as-is.
#         DIVERGE -> php disagrees; the guard hides a real divergence that needs
#                    a REASONED SKIPIF or a *_zend.phpt twin (never a bare skip).
#         TWIN    -> a *_zend.phpt twin pair member. A message-less skip is
#                    LEGITIMATE here: the pairing itself is the documented
#                    reason (base skips on php, _zend skips on PHL). No action.
#       The burn-down target is zero AGREES and zero DIVERGE lines.
#
#   build-aux/triage-bare-skips.sh --guards <out-file>
#       Existence-guard census. Extracts every symbol tested by a
#       function_exists/class_exists/interface_exists/method_exists/defined/
#       extension_loaded guard in a SKIPIF, then probes each symbol under BOTH
#       engines:
#         DEAD  -> present in both engines; the guard can never fire and is
#                  noise that obscures the real ones. Delete it.
#         LIVE  -> differs between engines (or missing in both); the guard is
#                  meaningful, but still needs a WRITTEN REASON.
#
# Run from the repo root. Both modes are read-only.

set -u

PHP_BIN=${PHP_BIN:-/usr/bin/php}
PHL_BIN=${PHL_BIN:-build/x86_64-linux-gnu/full/phl}

# Extract a test's SKIPIF body (between the --SKIPIF-- header and the next one).
skipif_body() {
  sed -n '/^--SKIPIF--/,/^--[A-Z]/p' "$1" | sed '1d;$d'
}

# A bare skip is any `skip` emission whose string ends right after the word:
# echo 'skip'; / echo "skip"; / print 'skip'; / die('skip'); / exit("skip");
# A message-carrying skip ("skip needs foo") is what we WANT and is not matched.
find_bare() {
  find tests/ph7 -name '*.phpt' | sort | while read -r f; do
    body=$(skipif_body "$f")
    [ -n "$body" ] || continue
    printf '%s\n' "$body" \
      | grep -qE "(echo|print|die|exit)[[:space:]]*\(?[[:space:]]*['\"]skip['\"][[:space:]]*\)?[[:space:]]*;" \
      && echo "$f"
  done
}

# A *_zend.phpt twin, or a base file that HAS a _zend twin beside it.
is_twin() {
  case $1 in
    *_zend.phpt) return 0 ;;
  esac
  [ -f "${1%.phpt}_zend.phpt" ]
}

triage_bare() {
  out=$1
  : > "$out"
  find_bare | while read -r f; do
    if is_twin "$f"; then
      echo "TWIN    $f" >> "$out"
      continue
    fi
    d=$(mktemp -d)
    base=$(basename "$f")
    python3 - "$f" "$d/$base" <<'PY'
import sys,re
src,dst=sys.argv[1],sys.argv[2]
s=open(src).read()
s=re.sub(r'--SKIPIF--\n.*?(?=\n--)', '', s, flags=re.S)
open(dst,'w').write(s)
PY
    res=$(timeout 25 "$PHP_BIN" tests/phpt.php --target-executable "$PHP_BIN" \
            --target-dir "$d" 2>&1 | grep -cE "^not ok")
    if [ "$res" = "0" ]; then echo "AGREES  $f"; else echo "DIVERGE $f"; fi >> "$out"
    rm -rf "$d"
  done
  echo "--- summary ---" >> "$out"
  for k in AGREES DIVERGE TWIN; do
    printf '%s %s\n' "$(grep -c "^$k" "$out")" "$k" >> "$out"
  done
}

# --- guard census ---------------------------------------------------------

guard_symbols() {
  find tests/ph7 -name '*.phpt' | while read -r f; do
    skipif_body "$f" \
      | grep -oE "(function_exists|class_exists|interface_exists|method_exists|extension_loaded|defined)[[:space:]]*\([[:space:]]*['\"][^'\"]+['\"]" \
      | sed -E "s/.*\([[:space:]]*['\"]//; s/['\"]\$//"
  done | sort -u
}

# Ask one engine whether a symbol exists. Probed with the same predicate family
# the guards use, so a class/interface/constant is not mistaken for missing.
probe_symbol() {
  "$1" -r 'if($argc<2)exit(2);$s=$argv[1];
    var_dump(function_exists($s)||class_exists($s)||interface_exists($s)||defined($s)||extension_loaded($s));' \
    -- "$2" 2>/dev/null | grep -q 'bool(true)' && echo yes || echo no
}

census_guards() {
  out=$1
  : > "$out"
  guard_symbols | while read -r sym; do
    p=$(probe_symbol "$PHP_BIN" "$sym")
    l=$(probe_symbol "$PHL_BIN" "$sym")
    n=$(grep -rl -- "$sym" tests/ph7 --include=*.phpt 2>/dev/null | wc -l | tr -d ' ')
    if [ "$p" = yes ] && [ "$l" = yes ]; then
      printf 'DEAD  %-34s php=%s phl=%s files=%s\n' "$sym" "$p" "$l" "$n" >> "$out"
    else
      printf 'LIVE  %-34s php=%s phl=%s files=%s\n' "$sym" "$p" "$l" "$n" >> "$out"
    fi
  done
  echo "--- summary ---" >> "$out"
  for k in DEAD LIVE; do
    printf '%s %s\n' "$(grep -c "^$k" "$out")" "$k" >> "$out"
  done
}

case ${1:-} in
  --guards) [ $# -ge 2 ] || { echo "usage: $0 --guards <out-file>" >&2; exit 2; }
            census_guards "$2" ;;
  "")       echo "usage: $0 [--guards] <out-file>" >&2; exit 2 ;;
  *)        triage_bare "$1" ;;
esac
