--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array element referencing a local keeps its VALUE when the frame goes
--FILE--
<?php
// The three ways an array holding a reference to a local outlives its frame.
function ref_elem_returned() { $v = 9; $a = [1, &$v, 3]; return $a; }
var_dump(ref_elem_returned());

function ref_elem_string_key() { $v = 9; $a = ['x' => 1, 'y' => &$v]; return $a; }
var_dump(ref_elem_string_key());

function ref_elem_copied() { $v = 9; $a = [1, &$v]; $b = $a; return $b; }
var_dump(ref_elem_copied());

function ref_elem_out(&$out) { $v = 9; $out = [1, &$v]; }
ref_elem_out($reo);
var_dump($reo);

// The local's last value is the one that survives, and nesting makes no difference.
function ref_elem_mutated() { $v = 1; $a = [&$v]; $v = 42; return $a; }
var_dump(ref_elem_mutated());

function ref_elem_nested() { $v = 1; return [[&$v]]; }
var_dump(ref_elem_nested());

// Two elements of the same dead local still share it.
function ref_elem_shared() { $v = 1; $a = [&$v]; $b = [&$v]; return [$a, $b]; }
$res = ref_elem_shared();
$res[0][0] = 7;
var_dump($res[1][0]);

// A parameter is a local like any other; so is a static's or a global's target.
function ref_elem_param($p) { return [&$p]; }
var_dump(ref_elem_param(5));

$reg = [];
function ref_elem_global() { global $reg; $v = 9; $reg['k'] = &$v; }
ref_elem_global();
var_dump($reg);
?>
--EXPECT--
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(9)
  [2]=>
  int(3)
}
array(2) {
  ["x"]=>
  int(1)
  ["y"]=>
  int(9)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(9)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(9)
}
array(1) {
  [0]=>
  int(42)
}
array(1) {
  [0]=>
  array(1) {
    [0]=>
    int(1)
  }
}
int(7)
array(1) {
  [0]=>
  int(5)
}
array(1) {
  ["k"]=>
  int(9)
}
--CLEAN--
<?php
unset($reo, $res, $reg);
