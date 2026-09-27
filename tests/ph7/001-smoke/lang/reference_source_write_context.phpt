--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `=&` source is a write context, and a call source is a notice
--FILE--
<?php
// The smoke corpus runs in ONE shared interpreter, so both of these have to be
// put back at the end -- raising the level for the rest of the run makes a later
// test report a diagnostic its expectation does not carry.
$rswcLevel = error_reporting(E_ALL);
set_error_handler(function ($no, $msg) {
    if (!(error_reporting() & $no)) { return false; }
    echo "[notice] $msg\n";
    return true;
});
// php compiles the SOURCE of a reference bind in write context, which is what
// makes `$r =& $undef` and `$r =& $a[5]` CREATE what they bind to. PHL read it
// instead, so both warned about what was missing and then refused the bind with
// "Reference operator require a variable not a constant as it's right operand",
// leaving the target undefined as well.

$rswcR =& $rswcUndef;
var_dump($rswcR, $rswcUndef);

$rswcA = [1, 2];
$rswcR2 =& $rswcA[5];
$rswcR2 = 9;
var_dump($rswcA[5]);

$rswcB = [];
$rswcR3 =& $rswcB['k']['j'];
$rswcR3 = 7;
var_dump($rswcB);

// A source with no slot at all binds a FRESH variable holding the value --
// silently when the call is only the BASE of the source...
function rswcArr() { return [1, 2]; }
function rswcObj() { return new RswcHolder; }
class RswcHolder { public $p = 4; }
$rswcR4 =& rswcArr()[0];
var_dump($rswcR4);
$rswcR5 =& rswcObj()->p;
var_dump($rswcR5);

// ...and with php's notice when the source IS the call and the callee does not
// return by reference. Every target shape raises it.
function rswcVal() { return 5; }
class RswcStat { public static $s = 0; public $p = 0; function m() { return 6; } }
$rswcR6 =& rswcVal();
var_dump($rswcR6);
$rswcO = new RswcStat;
$rswcR7 =& $rswcO->m();
var_dump($rswcR7);
$rswcC = [];
$rswcC[] =& rswcVal();
$rswcC[2] =& rswcVal();
var_dump($rswcC);
$rswcO->p =& rswcVal();
var_dump($rswcO->p);
RswcStat::$s =& rswcVal();
var_dump(RswcStat::$s);

// A by-reference function is the whole point of the exemption: no notice, and
// the alias is real.
function &rswcRef() { static $x = 1; return $x; }
$rswcR8 =& rswcRef();
$rswcR8 = 99;
var_dump(rswcRef());

// Ordinary binds are untouched.
$rswcV = 5;
$rswcR9 =& $rswcV;
$rswcR9 = 6;
var_dump($rswcV);
$rswcD = [1];
$rswcRa =& $rswcD[0];
$rswcRa = 3;
var_dump($rswcD);
restore_error_handler();
error_reporting($rswcLevel);
echo "END\n";
?>
--EXPECT--
NULL
NULL
int(9)
array(1) {
  ["k"]=>
  array(1) {
    ["j"]=>
    &int(7)
  }
}
int(1)
int(4)
[notice] Only variables should be assigned by reference
int(5)
[notice] Only variables should be assigned by reference
int(6)
[notice] Only variables should be assigned by reference
[notice] Only variables should be assigned by reference
array(2) {
  [0]=>
  int(5)
  [2]=>
  int(5)
}
[notice] Only variables should be assigned by reference
int(5)
[notice] Only variables should be assigned by reference
int(5)
int(99)
int(6)
array(1) {
  [0]=>
  &int(3)
}
END
