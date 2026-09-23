--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The printf family reads its arguments without converting them in place
--FILE--
<?php
// vsprintf/vprintf/vfprintf hand the formatter the $values array's own element
// slots, so an in-place coercion is a write the caller can see.
$sprintfRoArr = [false, 42, -0.5, "3.9"];
vsprintf("%o %s %b %d", $sprintfRoArr);
var_dump($sprintfRoArr);

$sprintfRoArr2 = [1.5, "0x1A"];
ob_start();
vprintf("%d %x", $sprintfRoArr2);
ob_end_clean();
var_dump($sprintfRoArr2);

// The same write is visible without an array: a positional argument reused by a
// second specifier would be read back already converted.
var_dump(sprintf('%1$d|%1$s', 3.9));
var_dump(sprintf('%1$d|%1$s', "3.9"));
var_dump(sprintf('%1$x|%1$s', "255abc"));
var_dump(sprintf('%1$s|%1$d|%1$f', "7.5"));
--EXPECT--
array(4) {
  [0]=>
  bool(false)
  [1]=>
  int(42)
  [2]=>
  float(-0.5)
  [3]=>
  string(3) "3.9"
}
array(2) {
  [0]=>
  float(1.5)
  [1]=>
  string(4) "0x1A"
}
string(5) "3|3.9"
string(5) "3|3.9"
string(9) "ff|255abc"
string(14) "7.5|7|7.500000"
--CLEAN--
<?php
