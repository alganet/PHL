--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: get_class_methods() lists a trait composition in php's order
--DESCRIPTION--
Inside one class php lists its own BODY methods first, then each used trait in
`use` order, and within a trait each of its methods in the TRAIT's declaration
order preceded by the aliases made from it — then the same again for each
ancestor. PHL sorted a level by declaration line, and a trait method's line is
the TRAIT's, so a whole composition sorted ahead of the class's own body and the
aliases landed wherever their copied line put them.
--FILE--
<?php
trait GcoT1 { public function a1() {} public function b1() {} }
trait GcoT2 { public function a2() {} public function b2() {} }

class GcoOne {
    public function own1() {}
    use GcoT1, GcoT2 { GcoT1::a1 as x1; GcoT2::b2 as x2; }
    public function own2() {}
}
/* Two aliases of the SAME method keep their adaptation-block order, ahead of it. */
class GcoTwo {
    use GcoT1 { b1 as z2; a1 as z1; a1 as y1; }
    public function own() {}
}
/* A subclass: its own level first, then the parent's — and a trait the subclass
   uses ITSELF composes at the subclass's level, while the parent's alias stays
   with the parent. */
class GcoKid extends GcoOne {
    public function kid() {}
    use GcoT1 { a1 as k1; }
}
var_dump(get_class_methods('GcoOne'));
var_dump(get_class_methods('GcoTwo'));
var_dump(get_class_methods('GcoKid'));
?>
--EXPECT--
array(8) {
  [0]=>
  string(4) "own1"
  [1]=>
  string(4) "own2"
  [2]=>
  string(2) "x1"
  [3]=>
  string(2) "a1"
  [4]=>
  string(2) "b1"
  [5]=>
  string(2) "a2"
  [6]=>
  string(2) "x2"
  [7]=>
  string(2) "b2"
}
array(6) {
  [0]=>
  string(3) "own"
  [1]=>
  string(2) "z1"
  [2]=>
  string(2) "y1"
  [3]=>
  string(2) "a1"
  [4]=>
  string(2) "z2"
  [5]=>
  string(2) "b1"
}
array(10) {
  [0]=>
  string(3) "kid"
  [1]=>
  string(2) "k1"
  [2]=>
  string(2) "a1"
  [3]=>
  string(2) "b1"
  [4]=>
  string(4) "own1"
  [5]=>
  string(4) "own2"
  [6]=>
  string(2) "x1"
  [7]=>
  string(2) "a2"
  [8]=>
  string(2) "x2"
  [9]=>
  string(2) "b2"
}
--CLEAN--
<?php
