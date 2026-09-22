--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
natsort()/natcasesort() COERCE their elements (they are asort() under SORT_NATURAL)
--DESCRIPTION--
PHL declared natsort()/natcasesort() in the prelude as `uasort($array, 'strnatcmp')`, and that
is a different function: uasort hands each element to a USERLAND callback, so every element had
to satisfy strnatcmp's `string` ZPP row. php's natsort coerces each element the way any string
comparison does, so an array as ordinary as `[10, "9", null]` was a TypeError in PHL and a
sorted array in php -- and an object with no __toString() answered strnatcmp's ZPP TypeError
where php answers the coercion Error. Both are C builtins now (ph7_hashmap_natsort), running
asort()'s merge sort under SORT_NATURAL (+ SORT_FLAG_CASE), which is how php implements them:
the flag comparator's HashmapFlagStringify already renders an array with php's "Array to string
conversion" warning and raises the coercion Error once per sort. ArrayObject/ArrayIterator's
methods delegate to them.
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

class NsBare {}

echo "== the everyday case: a natural sort keeps keys\n";
$a = ['img12.png', 'img10.png', 'img2.png', 'img1.png'];
var_dump(natsort($a));
print_r($a);

echo "== a MIXED array coerces instead of throwing\n";
$m = [10, "9", 2.5, true, null, "a1"];
natsort($m);
var_dump($m);

echo "== natcasesort folds case\n";
$c = ['IMG10', 'img2', 'IMG1', 'img12'];
natcasesort($c);
print_r($c);

echo "== an ARRAY element warns and renders as \"Array\"\n";
$w = ['a1', [1]];
natsort($w);
echo implode(",", array_keys($w)), "\n";

echo "== a not-stringable OBJECT is php's coercion Error\n";
$o = ['a1', new NsBare()];
try { natsort($o); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
$o2 = ['a1', new NsBare()];
try { natcasesort($o2); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== degenerate inputs\n";
$e = [];
var_dump(natsort($e), $e);
$one = ['b'];
var_dump(natsort($one), $one);

echo "== a non-array \$array is a TypeError\n";
$s = "x";
try { natsort($s); } catch (Throwable $t) { echo get_class($t), ": ", $t->getMessage(), "\n"; }
$i = 1;
try { natcasesort($i); } catch (Throwable $t) { echo get_class($t), ": ", $t->getMessage(), "\n"; }

echo "== arity\n";
try { natsort(); } catch (Throwable $t) { echo get_class($t), ": ", $t->getMessage(), "\n"; }
$z = [1];
try { natsort($z, 1); } catch (Throwable $t) { echo get_class($t), ": ", $t->getMessage(), "\n"; }

echo "== ArrayObject / ArrayIterator delegate\n";
$ao = new ArrayObject([10, null, "9"]);
$ao->natsort();
var_dump($ao->getArrayCopy());
$ai = new ArrayIterator(['IMG12', 'img2', 'IMG1']);
$ai->natcasesort();
print_r($ai->getArrayCopy());
?>
--EXPECT--
== the everyday case: a natural sort keeps keys
bool(true)
Array
(
    [3] => img1.png
    [2] => img2.png
    [1] => img10.png
    [0] => img12.png
)
== a MIXED array coerces instead of throwing
array(6) {
  [4]=>
  NULL
  [3]=>
  bool(true)
  [2]=>
  float(2.5)
  [1]=>
  string(1) "9"
  [0]=>
  int(10)
  [5]=>
  string(2) "a1"
}
== natcasesort folds case
Array
(
    [2] => IMG1
    [1] => img2
    [0] => IMG10
    [3] => img12
)
== an ARRAY element warns and renders as "Array"
  [2] Array to string conversion
1,0
== a not-stringable OBJECT is php's coercion Error
Error: Object of class NsBare could not be converted to string
Error: Object of class NsBare could not be converted to string
== degenerate inputs
bool(true)
array(0) {
}
bool(true)
array(1) {
  [0]=>
  string(1) "b"
}
== a non-array $array is a TypeError
TypeError: natsort(): Argument #1 ($array) must be of type array, string given
TypeError: natcasesort(): Argument #1 ($array) must be of type array, int given
== arity
ArgumentCountError: natsort() expects exactly 1 argument, 0 given
ArgumentCountError: natsort() expects exactly 1 argument, 2 given
== ArrayObject / ArrayIterator delegate
array(3) {
  [1]=>
  NULL
  [2]=>
  string(1) "9"
  [0]=>
  int(10)
}
Array
(
    [2] => IMG1
    [1] => img2
    [0] => IMG12
)
