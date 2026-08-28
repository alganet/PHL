--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ArrayObject (band D SPL slice 1)
--FILE--
<?php
$ao = new ArrayObject(['a' => 1, 'b' => 2]);
foreach ($ao as $k => $v) { echo "$k=$v;"; }
echo "\n";
echo get_class($ao->getIterator()), count($ao), "\n";
$ao['c'] = 3;
$ao->append(9);
unset($ao['a']);
print_r($ao->getArrayCopy());
$old = $ao->exchangeArray(['z' => 26]);
print_r($old);
print_r($ao->getArrayCopy());
echo $ao->getIteratorClass(), "\n";
// ARRAY_AS_PROPS maps property access onto the array
$q = new ArrayObject(['k' => 1], ArrayObject::ARRAY_AS_PROPS);
$q->k2 = 5;
echo $q['k2'], $q->k, "\n";
echo isset($q->k) ? 'T' : 'F', isset($q->nope) ? 'T' : 'F', "\n";
unset($q->k);
echo isset($q['k']) ? 'T' : 'F', "\n";
echo $q->missing ?? 'NC', "\n";
echo ArrayObject::STD_PROP_LIST, ArrayObject::ARRAY_AS_PROPS,
     ArrayIterator::STD_PROP_LIST, ArrayIterator::ARRAY_AS_PROPS, "\n";
try {
    new ArrayObject(3);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $ao->setIteratorClass('stdClass');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $ao->setIteratorClass('OaoNopeClass');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
class OaoMyIt extends ArrayIterator {}
$ao->setIteratorClass('OaoMyIt');
echo get_class($ao->getIterator()), "\n";
// sorting through the object
$s = new ArrayObject(['b' => 2, 'a' => 1]);
$s->ksort();
print_r($s->getArrayCopy());
$s->uasort(fn ($x, $y) => $y <=> $x);
print_r($s->getArrayCopy());
echo (new ArrayObject([]) instanceof IteratorAggregate) ? 'T' : 'F';
echo (new ArrayObject([]) instanceof ArrayAccess) ? 'T' : 'F';
echo (new ArrayObject([]) instanceof Countable) ? 'T' : 'F', "\n";
?>
--EXPECT--
a=1;b=2;
ArrayIterator2
Array
(
    [b] => 2
    [c] => 3
    [0] => 9
)
Array
(
    [b] => 2
    [c] => 3
    [0] => 9
)
Array
(
    [z] => 26
)
ArrayIterator
51
TF
F
NC
1212
ArrayObject::__construct(): Argument #1 ($array) must be of type array, int given
ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, stdClass given
ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, OaoNopeClass given
OaoMyIt
Array
(
    [a] => 1
    [b] => 2
)
Array
(
    [b] => 2
    [a] => 1
)
TTT
--CLEAN--
<?php
unset($ao, $old, $q, $s, $k, $v);
