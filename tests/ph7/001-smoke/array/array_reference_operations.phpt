--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with references
--SKIPIF--
<?php
// php propagates an element's reference marker through array COPIES: after
// $b = &$a[1], both array_merge($a,...) and array_slice(...) still var_dump that
// element as &int(2). PHL copies the value and loses the is_ref bit, so only the
// ORIGINAL array shows the marker. Recorded in NEWPLAN section 7; engine-specific
// until reference propagation through array copies is implemented.
if (function_exists('zend_version')) echo 'skip PHL does not propagate is_ref through array copies';
?>
--FILE--
<?php
// Test array operations that involve references to exercise uncovered code paths
$a = array(1, 2, 3);
$b = &$a[1]; // Reference to array element
$c = array(4, 5, 6);
$d = array_merge($a, $c); // Merge arrays
$e = array_slice($d, 1, 2); // Slice array
var_dump($a, $b, $d, $e);
?>
--EXPECT--
array(3) {
  [0]=>
  int(1)
  [1]=>
  &int(2)
  [2]=>
  int(3)
}
int(2)
array(6) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
  [5]=>
  int(6)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
--CLEAN--
<?php
unset($a, $b, $c, $d, $e);
