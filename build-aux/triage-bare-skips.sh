#!/bin/sh
# Triage the corpus's bare-`skip` SKIPIFs (burn-down).
#
# A test whose SKIPIF is `echo 'skip'` with NO reason never runs under the php
# oracle and nobody can tell whether it is guarding a real divergence or just
# hiding one. That is not hypothetical: smoke/oo/object_cast_to_array.phpt sat
# behind such a guard asserting PHL's WRONG (array)-cast keys, and only a
# Windows run surfaced it (19 Jul 2026).
#
# For each bare-skip test this strips the SKIPIF and runs the test under php:
#   AGREES  -> php produces the expected output; the guard is unnecessary and
#              the test can go cross-engine as-is.
#   DIVERGE -> php disagrees; the guard hides a real divergence that needs a
#              REASONED SKIPIF or a *_zend.phpt twin pair (never a bare skip).
#
# Usage:  build-aux/triage-bare-skips.sh <out-file>    (run from the repo root)
#
# Note *_zend.phpt twins legitimately carry a message-less skip: the pairing
# itself is the documented reason (base skips on php, _zend skips on PHL).

find_bare() {
  grep -rln "zend_version" tests/ph7 --include=*.phpt | while read -r f; do
    sed -n '/--SKIPIF--/,/^--/p' "$f" | grep -qE "echo *'skip' *;|echo *\"skip\" *;" && echo "$f"
  done
}
OUT=$1
: > "$OUT"
while read -r f; do
  d=$(mktemp -d)
  base=$(basename "$f")
  python3 - "$f" "$d/$base" <<'PY'
import sys,re
src,dst=sys.argv[1],sys.argv[2]
s=open(src).read()
s=re.sub(r'--SKIPIF--\n.*?(?=\n--)', '', s, flags=re.S)
open(dst,'w').write(s)
PY
  res=$(timeout 25 /usr/bin/php tests/phpt.php --target-executable /usr/bin/php --target-dir "$d" 2>&1 | grep -cE "^not ok")
  if [ "$res" = "0" ]; then echo "AGREES  $f"; else echo "DIVERGE $f"; fi >> "$OUT"
  rm -rf "$d"
done <<EOT
$(find_bare)
EOT
