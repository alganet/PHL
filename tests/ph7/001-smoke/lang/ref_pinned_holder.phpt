--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure capture or a reference-bound property is a holder like any other
--FILE--
<?php
// A `use (&$x)` capture keeps the element it shares a reference.
$rpv = 1;
$rpf = function () use (&$rpv) { return $rpv; };
$rpa = [&$rpv];
unset($rpv);
var_dump($rpa);
$rpa[0] = 5;
var_dump($rpf());

// So does a property bound by reference to an element.
class RefPinnedBox { public $p; }
$rpb = [7];
$rpo = new RefPinnedBox;
$rpo->p = &$rpb[0];
var_dump($rpb);
$rpc = $rpb;
$rpc[0] = 9;
var_dump($rpb[0], $rpo->p);
unset($rpb);
var_dump($rpo->p);

// ...and a static property.
class RefPinnedStore { public static $s; }
$rpd = [3];
RefPinnedStore::$s = &$rpd[0];
var_dump($rpd);
RefPinnedStore::$s = 4;
var_dump($rpd[0]);
?>
--EXPECT--
array(1) {
  [0]=>
  &int(1)
}
int(5)
array(1) {
  [0]=>
  &int(7)
}
int(9)
int(9)
int(9)
array(1) {
  [0]=>
  &int(3)
}
int(4)
--CLEAN--
<?php
unset($rpa, $rpf, $rpb, $rpc, $rpo, $rpd);
