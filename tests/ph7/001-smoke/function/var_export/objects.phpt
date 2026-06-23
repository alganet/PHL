--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export() objects: __set_state, visibility (bare names), nesting
--FILE--
<?php

class VeVis { public $a=1; protected $b=2; private $c=3; }
var_export(new VeVis); echo "\n";
class VeNest { public $arr=[1,[2,3]]; public $s="hi"; }
var_export(new VeNest); echo "\n";
class VeEmpty {}
var_export(new VeEmpty); echo "\n";
class VeInner { public $x=1; }
class VeOuter { public $o; }
$o = new VeOuter; $o->o = new VeInner;
var_export($o); echo "\n";
?>
--EXPECT--
\VeVis::__set_state(array(
   'a' => 1,
   'b' => 2,
   'c' => 3,
))
\VeNest::__set_state(array(
   'arr' => 
  array (
    0 => 1,
    1 => 
    array (
      0 => 2,
      1 => 3,
    ),
  ),
   's' => 'hi',
))
\VeEmpty::__set_state(array(
))
\VeOuter::__set_state(array(
   'o' => 
  \VeInner::__set_state(array(
     'x' => 1,
  )),
))
