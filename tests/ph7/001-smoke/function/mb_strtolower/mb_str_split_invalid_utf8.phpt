--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RECORDED DIVERGENCE (malformed input only): mb_str_split() groups an ill-formed UTF-8 run as the same number of characters mb_strlen() counts, so count(mb_str_split($s)) === mb_strlen($s) holds. php's own grouping swallows the byte that FOLLOWS the run, and then disagrees with its own mb_strlen (it reports 2 characters for "\xe0\xa0c" and splits it into 1) — the _zend twin pins php's numbers. Both engines return the same BYTES; only the grouping differs, and only for input that is not UTF-8.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
$t = ["\xe0\xa0\x63", "\xe0\x63", "\xc3\x63", "\xf0\x9f\x98\x63", "\xff\x63",
      "\xed\xa0\x63", "\xe0\xa0", "a\xffb"];
foreach ($t as $s) {
  $parts = mb_str_split($s);
  printf("%-12s strlen=%d count=%d split=%s\n", bin2hex($s), mb_strlen($s),
    count($parts), json_encode(array_map('bin2hex', $parts)));
  // The pieces always reassemble into the input.
  var_dump(implode('', $parts) === $s);
}
?>
--EXPECT--
e0a063       strlen=2 count=2 split=["e0a0","63"]
bool(true)
e063         strlen=2 count=2 split=["e0","63"]
bool(true)
c363         strlen=2 count=2 split=["c3","63"]
bool(true)
f09f9863     strlen=2 count=2 split=["f09f98","63"]
bool(true)
ff63         strlen=2 count=2 split=["ff","63"]
bool(true)
eda063       strlen=3 count=3 split=["ed","a0","63"]
bool(true)
e0a0         strlen=1 count=1 split=["e0a0"]
bool(true)
61ff62       strlen=3 count=3 split=["61","ff","62"]
bool(true)
--CLEAN--
<?php
