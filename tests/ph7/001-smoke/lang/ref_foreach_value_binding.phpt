--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-reference foreach binds a real reference that outlives the loop
--FILE--
<?php
// The value variable is still bound to the LAST element when the loop ends.
$rfa = [1, 2, 3];
foreach ($rfa as &$rfv) {}
var_dump(isset($rfv), $rfv);
var_dump($rfa);

// So writing through it writes into the array — the famous second-loop gotcha.
$rfb = [1, 2, 3];
foreach ($rfb as &$rfw) {}
foreach ($rfb as $rfw) {}
var_dump($rfb);

// ...and unset() is what undoes it.
$rfc = [1, 2, 3];
foreach ($rfc as &$rfx) {}
unset($rfx);
foreach ($rfc as $rfx) {}
var_dump($rfc);

// The element is a REFERENCE while the loop variable holds it, so a copy shares it.
$rfd = [1, 2, 3];
foreach ($rfd as &$rfy) {}
$rfe = $rfd;
$rfe[2] = 99;
var_dump($rfd[2]);
unset($rfy);

// Object iteration binds the same way.
class RefForeachPoint { public $x = 1; public $y = 2; }
$rfo = new RefForeachPoint;
foreach ($rfo as &$rfz) {}
var_dump($rfz);
$rfz = 20;
var_dump($rfo->y);
unset($rfz);
?>
--EXPECT--
bool(true)
int(3)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  &int(3)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  &int(2)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
int(99)
int(2)
int(20)
--CLEAN--
<?php
unset($rfa, $rfv, $rfb, $rfw, $rfc, $rfx, $rfd, $rfe, $rfo);
