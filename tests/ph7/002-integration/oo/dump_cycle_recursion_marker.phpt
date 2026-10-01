--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A container that is its own descendant dumps as *RECURSION*
--DESCRIPTION--
The three dumpers share one protection mark, and it is an ANCESTOR test rather than
a "seen before" test: var_dump replaces the whole value with the marker, print_r
prints the container header and then the marker in place of the body, and var_export
-- which has no evaluable expression for a cycle -- warns and writes NULL. A value
reached twice down two sibling branches is not a cycle and renders in full both times.
--FILE--
<?php
class CycleDumpNode { public $name; public $link; public $n = 1; }

echo "== object that is its own property ==\n";
$a = new CycleDumpNode; $a->name = 'a'; $a->link = $a;
var_dump($a);
print_r($a);
echo "\n";
var_export($a);
echo "\n";

echo "== two objects pointing at each other ==\n";
$x = new CycleDumpNode; $x->name = 'x';
$y = new CycleDumpNode; $y->name = 'y';
$x->link = $y; $y->link = $x;
var_dump($x);
print_r($x);
echo "\n";

echo "== an array that is its own element ==\n";
$arr = ['k' => 1];
$arr['self'] = &$arr;
var_dump($arr);
print_r($arr);
echo "\n";
var_export($arr);
echo "\n";

echo "== the cycle closes through an array under an object ==\n";
$o = new CycleDumpNode; $o->name = 'o';
$o->link = ['deep' => ['deeper' => $o]];
var_dump($o);
print_r($o);
echo "\n";
var_export($o);
echo "\n";

echo "== a value reached twice is NOT a cycle ==\n";
$shared = new CycleDumpNode; $shared->name = 's';
var_dump([$shared, $shared]);
$sub = [1, 2];
var_dump([$sub, $sub]);
?>
--EXPECT--
== object that is its own property ==
object(CycleDumpNode)#1 (3) {
  ["name"]=>
  string(1) "a"
  ["link"]=>
  *RECURSION*
  ["n"]=>
  int(1)
}
CycleDumpNode Object
(
    [name] => a
    [link] => CycleDumpNode Object
 *RECURSION*
    [n] => 1
)

\CycleDumpNode::__set_state(array(
   'name' => 'a',
   'link' => NULL,
   'n' => 1,
))
== two objects pointing at each other ==
object(CycleDumpNode)#2 (3) {
  ["name"]=>
  string(1) "x"
  ["link"]=>
  object(CycleDumpNode)#3 (3) {
    ["name"]=>
    string(1) "y"
    ["link"]=>
    *RECURSION*
    ["n"]=>
    int(1)
  }
  ["n"]=>
  int(1)
}
CycleDumpNode Object
(
    [name] => x
    [link] => CycleDumpNode Object
        (
            [name] => y
            [link] => CycleDumpNode Object
 *RECURSION*
            [n] => 1
        )

    [n] => 1
)

== an array that is its own element ==
array(2) {
  ["k"]=>
  int(1)
  ["self"]=>
  *RECURSION*
}
Array
(
    [k] => 1
    [self] => Array
 *RECURSION*
)

array (
  'k' => 1,
  'self' => NULL,
)
== the cycle closes through an array under an object ==
object(CycleDumpNode)#4 (3) {
  ["name"]=>
  string(1) "o"
  ["link"]=>
  array(1) {
    ["deep"]=>
    array(1) {
      ["deeper"]=>
      *RECURSION*
    }
  }
  ["n"]=>
  int(1)
}
CycleDumpNode Object
(
    [name] => o
    [link] => Array
        (
            [deep] => Array
                (
                    [deeper] => CycleDumpNode Object
 *RECURSION*
                )

        )

    [n] => 1
)

\CycleDumpNode::__set_state(array(
   'name' => 'o',
   'link' => 
  array (
    'deep' => 
    array (
      'deeper' => NULL,
    ),
  ),
   'n' => 1,
))
== a value reached twice is NOT a cycle ==
array(2) {
  [0]=>
  object(CycleDumpNode)#5 (3) {
    ["name"]=>
    string(1) "s"
    ["link"]=>
    NULL
    ["n"]=>
    int(1)
  }
  [1]=>
  object(CycleDumpNode)#5 (3) {
    ["name"]=>
    string(1) "s"
    ["link"]=>
    NULL
    ["n"]=>
    int(1)
  }
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
  [1]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
}
--EXPECT_STDERR--
PHP Warning:  var_export does not handle circular references in %s on line 9
PHP Warning:  var_export does not handle circular references in %s on line 26
PHP Warning:  var_export does not handle circular references in %s on line 35
