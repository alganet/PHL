--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export() circular references render as NULL (no infinite recursion)
--FILE--
<?php

set_error_handler(function($n, $s){ return true; }); // swallow the circular-ref warning
$a = [1, 2]; $a[2] = &$a;
var_export($a); echo "\n";
class VeCycle { public $child; public $v = 7; }
$o = new VeCycle; $o->child = $o;
var_export($o); echo "\n";
$x = [1, 2];                 // shared but NOT circular -> fully expanded twice
var_export([$x, $x]); echo "\n";
restore_error_handler();
?>
--EXPECT--
array (
  0 => 1,
  1 => 2,
  2 => NULL,
)
\VeCycle::__set_state(array(
   'child' => NULL,
   'v' => 7,
))
array (
  0 => 
  array (
    0 => 1,
    1 => 2,
  ),
  1 => 
  array (
    0 => 1,
    1 => 2,
  ),
)
