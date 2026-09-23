--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An existing name is RE-BOUND by `=&`, not refused
--FILE--
<?php
// A name that already holds a value is re-bound; the old value goes.
$rrx = 1; $rry = 2;
$rry = &$rrx;
$rry = 5;
var_dump($rrx, $rry);

// A name that already holds a REFERENCE is re-pointed; the first target is left alone.
$rra = 1; $rrb = 2;
$rrr = &$rra;
$rrr = &$rrb;
$rrr = 9;
var_dump($rra, $rrb);

// The `$ref = &$arr[$k]` idiom re-binds on every step of the loop.
$rrarr = [1, 2, 3];
foreach ($rrarr as $rrk => $rrv) {
    $rrref = &$rrarr[$rrk];
    $rrref = $rrref * 10;
}
unset($rrref);
var_dump($rrarr);

// A by-reference PARAMETER re-bound inside the callee stops writing to the caller.
function ref_rebind_param(&$p) { $local = 7; $p = &$local; $p = 3; }
$rrw = 1;
ref_rebind_param($rrw);
var_dump($rrw);

// $GLOBALS['name'] =& $var re-binds an existing global the same way.
$rrg1 = 1; $rrg2 = 2;
$GLOBALS['rrg2'] = &$rrg1;
$rrg2 = 6;
var_dump($rrg1, $rrg2);
?>
--EXPECT--
int(5)
int(5)
int(1)
int(9)
array(3) {
  [0]=>
  int(10)
  [1]=>
  int(20)
  [2]=>
  int(30)
}
int(1)
int(6)
int(6)
--CLEAN--
<?php
unset($rrx, $rry, $rra, $rrb, $rrr, $rrarr, $rrk, $rrv, $rrw, $rrg1, $rrg2);
