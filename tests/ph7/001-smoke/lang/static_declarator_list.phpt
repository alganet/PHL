--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
static declares a LIST, each element with its own optional initializer
--DESCRIPTION--
`static $a, $b = 2, $c;` is one statement declaring three slots in php, and it is
how two counters in one function are ordinarily written. PHL took the first
declarator and then refused the comma -- `Fatal error: static: Unexpected token
','` -- so the spelling did not compile at all. Each element keeps its own
initializer, an element without one starts NULL, and every slot persists across
calls exactly as a single declaration does.
--FILE--
<?php
function statListCounters() {
    static $statA, $statB = 2, $statC = 'k';
    $statA++;
    $statB *= 2;
    return [$statA, $statB, $statC];
}
var_dump(statListCounters(), statListCounters(), statListCounters());

class StatListHolder {
    public function tick() {
        static $statM, $statN = 10;
        $statM++;
        $statN--;
        return [$statM, $statN];
    }
}
$statObj = new StatListHolder();
var_dump($statObj->tick(), $statObj->tick());

/* At file scope the list is accepted too, and answers what a single one does. */
static $statTop, $statTop2 = 5;
var_dump($statTop, $statTop2);
?>
--EXPECT--
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(4)
  [2]=>
  string(1) "k"
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(8)
  [2]=>
  string(1) "k"
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(16)
  [2]=>
  string(1) "k"
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(9)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(8)
}
NULL
int(5)
