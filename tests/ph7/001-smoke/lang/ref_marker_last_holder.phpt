--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An element stops being a REFERENCE when its last co-holder goes
--FILE--
<?php
// A foreign element is a reference only while the variable it points at is alive.
$rmv = 10;
$rma = [&$rmv];
var_dump($rma);
unset($rmv);
var_dump($rma);

// Same rule from the other side: a reference taken TO an element.
$rmb = [1];
$rmr = &$rmb[0];
var_dump($rmb);
unset($rmr);
var_dump($rmb);

// Two arrays sharing one slot still hold each other after the variable goes.
$rmx = 1;
$rmc = [&$rmx];
$rmd = [&$rmx];
unset($rmx);
$rmc[0] = 5;
var_dump($rmc, $rmd);

// The half that is not cosmetic: a COPY of an element with no co-holder left is a
// copy, not a share.
$rmh = 7;
$rmi = [&$rmh];
unset($rmh);
$rmj = $rmi;
$rmj[0] = 99;
var_dump($rmi, $rmj);
?>
--EXPECT--
array(1) {
  [0]=>
  &int(10)
}
array(1) {
  [0]=>
  int(10)
}
array(1) {
  [0]=>
  &int(1)
}
array(1) {
  [0]=>
  int(1)
}
array(1) {
  [0]=>
  &int(5)
}
array(1) {
  [0]=>
  &int(5)
}
array(1) {
  [0]=>
  int(7)
}
array(1) {
  [0]=>
  int(99)
}
--CLEAN--
<?php
unset($rmv, $rma, $rmb, $rmr, $rmc, $rmd, $rmx, $rmh, $rmi, $rmj);
