--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionObject basics and iterable/trait detection
--FILE--
<?php
trait ReflObjTrait {
    public function fromTrait() { return 'trait'; }
}
class ReflObjIter implements Iterator {
    use ReflObjTrait;
    public function current(): mixed { return null; }
    public function key(): mixed { return null; }
    public function next(): void {}
    public function rewind(): void {}
    public function valid(): bool { return false; }
}

$o = new ReflObjIter();
$ro = new ReflectionObject($o);
echo $ro->getName(), "\n";
echo get_class($ro), "\n";
echo $ro->isInstance($o) ? 'inst' : 'not-inst', "\n";
echo $ro->isIterable() ? 'iterable' : 'not-iterable', "\n";
echo $ro->isIterateable() ? 'iterateable' : 'not-iterateable', "\n";
echo implode(',', $ro->getTraitNames()), "\n";
$rc = new ReflectionClass($o);
echo $rc->getName(), "\n";
$names = $rc->getInterfaceNames();
sort($names);
echo implode(',', $names), "\n";
?>
--EXPECT--
ReflObjIter
ReflectionObject
inst
iterable
iterateable
ReflObjTrait
ReflObjIter
Iterator,Traversable
