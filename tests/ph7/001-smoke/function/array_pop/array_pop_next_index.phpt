--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pop() gives the auto-index back when the popped element held the top one
--FILE--
<?php
// array_pop gives the auto-index back, so a push/pop stack reuses the slot instead
// of leaving a hole where count()-1 no longer names the top.
$apn_a = [];
$apn_a[] = 'x';
$apn_a[] = 'y';
array_pop($apn_a);
$apn_a[] = 'z';
var_dump(array_keys($apn_a), $apn_a[count($apn_a) - 1]);
// Only the TOP index comes back, and only when the popped key is the top one.
$apn_b = [5 => 'a', 6 => 'b'];
array_pop($apn_b);
$apn_b[] = 'c';
var_dump(array_keys($apn_b));
$apn_c = [10 => 'a'];
array_pop($apn_c);
$apn_c[] = 'b';
var_dump(array_keys($apn_c));
$apn_d = ['a', 'b'];
array_pop($apn_d);
array_pop($apn_d);
$apn_d[] = 'z';
var_dump(array_keys($apn_d));
// A string key on top leaves the counter alone...
$apn_e = [0 => 'a', 'k' => 'b'];
array_pop($apn_e);
$apn_e[] = 'c';
var_dump(array_keys($apn_e));
// ...and so does unset(), in php too.
$apn_f = [0 => 'a', 1 => 'b', 2 => 'c'];
unset($apn_f[2]);
$apn_f[] = 'd';
var_dump(array_keys($apn_f));
// Popping something that is not the top index does not move it either.
$apn_g = [3 => 'a'];
$apn_g[] = 'b';
$apn_g[1] = 'c';
array_pop($apn_g);
$apn_g[] = 'd';
var_dump(array_keys($apn_g));
?>
--EXPECT--
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(1)
}
string(1) "z"
array(2) {
  [0]=>
  int(5)
  [1]=>
  int(6)
}
array(1) {
  [0]=>
  int(10)
}
array(1) {
  [0]=>
  int(0)
}
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(1)
}
array(3) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(3)
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(4)
  [2]=>
  int(5)
}
