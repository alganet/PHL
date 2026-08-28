--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ArrayIterator (band D SPL slice 1)
--FILE--
<?php
$it = new ArrayIterator(['a' => 1, 'b' => 2, 5 => 'x']);
foreach ($it as $k => $v) { echo "$k=$v;"; }
echo "\n";
echo count($it), $it->count(), "\n";
echo $it['a'], $it->offsetExists('a') ? 'y' : 'n', $it->offsetExists('zz') ? 'y' : 'n', "\n";
// offsetExists is array_key_exists-like: a null value still exists
$nul = new ArrayIterator(['n' => null]);
echo $nul->offsetExists('n') ? 'y' : 'n', "\n";
$it['c'] = 9;
$it[] = 'app';
unset($it['a']);
print_r($it->getArrayCopy());
$it->seek(1);
echo $it->key(), '=', $it->current(), "\n";
try {
    $it->seek(99);
} catch (OutOfBoundsException $e) {
    echo $e->getMessage(), "\n";
}
try {
    $it->seek(-1);
} catch (OutOfBoundsException $e) {
    echo $e->getMessage(), "\n";
}
$it->rewind();
echo $it->key(), "\n";
echo $it->asort() ? 'T' : 'F', "\n";
print_r($it->getArrayCopy());
$it->ksort();
print_r($it->getArrayCopy());
echo $it->getFlags(), "\n";
try {
    new ArrayIterator('nope');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
$e2 = new ArrayIterator([]);
echo $e2->valid() ? 'T' : 'F', var_export($e2->key(), true), var_export($e2->current(), true), "\n";
// natural sort: methods and the global natsort()/natcasesort()
$nv = new ArrayIterator(['a' => 'img12', 'b' => 'img2']);
$nv->natsort();
print_r($nv->getArrayCopy());
$arr = ['img12', 'img10', 'img2'];
natsort($arr);
print_r($arr);
$arr2 = ['IMG12', 'img10'];
natcasesort($arr2);
print_r($arr2);
echo strnatcmp('img12', 'img10'), strnatcmp('img2', 'img10'), strnatcasecmp('IMG2', 'img10'), strnatcmp('a', 'a'), "\n";
echo strnatcmp('x01', 'x1'), strnatcmp('x 1', 'x1'), "\n";
// interface wiring + independent re-iteration
echo (new ArrayIterator([]) instanceof SeekableIterator) ? 'T' : 'F';
echo (new ArrayIterator([]) instanceof Iterator) ? 'T' : 'F';
echo (new ArrayIterator([]) instanceof Traversable) ? 'T' : 'F', "\n";
$it2 = new ArrayIterator([1, 2]);
foreach ($it2 as $x) { echo $x; }
foreach ($it2 as $x) { echo $x; }
echo "\n";
// mixed int/string ksort follows php 8 comparison rules
$mk = ['b' => 2, 5 => 1, 'c' => 4, 6 => 3, '10' => 9];
ksort($mk);
echo implode(',', array_keys($mk)), '|';
krsort($mk);
echo implode(',', array_keys($mk)), "\n";
?>
--EXPECT--
a=1;b=2;5=x;
33
1yn
y
Array
(
    [b] => 2
    [5] => x
    [c] => 9
    [6] => app
)
5=x
Seek position 99 is out of range
Seek position -1 is out of range
b
T
Array
(
    [b] => 2
    [c] => 9
    [6] => app
    [5] => x
)
Array
(
    [5] => x
    [6] => app
    [b] => 2
    [c] => 9
)
0
ArrayIterator::__construct(): Argument #1 ($array) must be of type array, string given
FNULLNULL
Array
(
    [b] => img2
    [a] => img12
)
Array
(
    [2] => img2
    [1] => img10
    [0] => img12
)
Array
(
    [1] => img10
    [0] => IMG12
)
1-1-10
-10
TTT
1212
5,6,10,b,c|c,b,10,6,5
--CLEAN--
<?php
unset($it, $nul, $e2, $nv, $arr, $arr2, $it2, $mk, $k, $v, $x);
