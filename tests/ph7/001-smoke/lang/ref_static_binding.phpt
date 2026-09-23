--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static's storage belongs to the function, not to the call that unsets its name
--FILE--
<?php
// unset() drops the NAME; the static keeps counting.
function ref_static_counter() {
    static $s = 1;
    $s++;
    $v = $s;
    unset($s);
    return $v;
}
var_dump(ref_static_counter(), ref_static_counter(), ref_static_counter());

// An element sharing a static is a reference, and stays one after the call.
function ref_static_share() {
    static $s = 1;
    $a = [&$s];
    $s = 2;
    return $a;
}
var_dump(ref_static_share());

// A `use (&$x)` capture is a holder too: unsetting the variable leaves the value.
$rsx = 1;
$rsf = function () use (&$rsx) { return $rsx; };
unset($rsx);
var_dump($rsf());
?>
--EXPECT--
int(2)
int(3)
int(4)
array(1) {
  [0]=>
  &int(2)
}
int(1)
--CLEAN--
<?php
unset($rsf);
