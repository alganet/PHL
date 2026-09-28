--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
headers_sent writes both out-params, before and after output
--FILE--
<?php
// Everything is COLLECTED and printed at the end: the first byte written is
// what the answer is about.
$hsBefore = [headers_sent($hsF0, $hsL0), $hsF0, $hsL0];
echo "";
$hsAfterEmpty = [headers_sent($hsF1, $hsL1), $hsF1, $hsL1];
echo "x";
$hsAfter = [headers_sent($hsF2, $hsL2), $hsF2, $hsL2];
$hsAfter[1] = str_replace(__FILE__, 'FILE', $hsAfter[1]);
echo "\n";
var_dump($hsBefore, $hsAfterEmpty, $hsAfter);
?>
--EXPECT--
x
array(3) {
  [0]=>
  bool(false)
  [1]=>
  string(0) ""
  [2]=>
  int(0)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  string(0) ""
  [2]=>
  int(0)
}
array(3) {
  [0]=>
  bool(true)
  [1]=>
  string(4) "FILE"
  [2]=>
  int(7)
}
--CLEAN--
<?php
unset($hsBefore, $hsAfterEmpty, $hsAfter, $hsF0, $hsL0, $hsF1, $hsL1, $hsF2, $hsL2);
