--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayObject/ArrayIterator serialize as php's four-element list, not their hidden slots
--DESCRIPTION--
php's payload for these two is a LIST -- [flags, storage, members, iterator class] -- and
nothing else can express it: the state lives in ext/spl's own struct, so there are no
properties to walk. PHL walked its own HIDDEN slots instead, which round-tripped inside PHL
and could neither read nor be read by php.
Two details the pair carries. The last element is NULL when the iterator class is the default
ArrayIterator, and always NULL for an ArrayIterator payload -- php shares one C body between
both classes, so ArrayIterator carries a slot it has no use for. And the iterator-class check
on the RESTORE is looser than setIteratorClass()'s: restoring accepts any Iterator where the
setter and the constructor demand a class derived from ArrayIterator; php words both refusals
with `ArrayObject`, even when ArrayIterator is the receiver.
Its neighbour, found by serializing the flags word: php masks the flags on the WRITE
(`~SPL_ARRAY_INT_MASK`), so `setFlags(-1)` then `getFlags()` is 65535 and not -1.
--FILE--
<?php
function aoShow($label, $fn) {
    try { $out = $fn(); if (!is_string($out)) { $out = var_export($out, true); } }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace(["\n", "\0"], ['', '^@'], $out), "\n";
}
class AoSerIt implements Iterator {
    public function current(): mixed { return 1; }
    public function key(): mixed { return 1; }
    public function next(): void {}
    public function rewind(): void {}
    public function valid(): bool { return false; }
}
class AoSerKid extends ArrayObject { public $p = 1; }

echo "-- the payload is php's list\n";
aoShow('ArrayObject', fn() => serialize(new ArrayObject([1, 'k' => 2])));
aoShow('ArrayIterator', fn() => serialize(new ArrayIterator([1, 'k' => 2])));
aoShow('with flags', fn() => json_encode((new ArrayIterator(['k' => 1], 2))->__serialize()));
aoShow('with an iterator class',
    fn() => json_encode((new ArrayObject(['k' => 1], 1, 'RecursiveArrayIterator'))->__serialize()));
aoShow('subclass members', fn() => serialize(new AoSerKid([7])));

echo "-- and it round-trips\n";
aoShow('ArrayObject', fn() => json_encode(unserialize(serialize(new ArrayObject([1, 'k' => 2])))->getArrayCopy()));
aoShow('ArrayIterator', fn() => json_encode(unserialize(serialize(new ArrayIterator([1, 'k' => 2])))->getArrayCopy()));
aoShow('subclass', function () {
    $o = new AoSerKid([7]);
    $o->p = 9;
    $b = unserialize(serialize($o));
    return get_class($b) . '|' . $b->p . '|' . json_encode($b->getArrayCopy());
});
aoShow('iterator class survives', function () {
    $o = new ArrayObject([], 0, 'RecursiveArrayIterator');
    return unserialize(serialize($o))->getIteratorClass();
});

echo "-- a payload PHP wrote is readable\n";
aoShow('php bytes', fn() => json_encode(unserialize(
    'O:11:"ArrayObject":4:{i:0;i:0;i:1;a:2:{i:0;i:1;s:1:"k";i:2;}i:2;a:0:{}i:3;N;}')
    ->getArrayCopy()));
aoShow('flags come back', function () {
    $a = new ArrayIterator;
    $a->__unserialize([2, ['x' => 1], [], null]);
    return $a->getFlags() . '|' . json_encode($a->getArrayCopy());
});

echo "-- the restore accepts any Iterator, the setter does not\n";
aoShow('restore a plain Iterator', function () {
    $a = new ArrayObject;
    $a->__unserialize([0, [], [], 'AoSerIt']);
    return $a->getIteratorClass();
});
aoShow('setIteratorClass refuses it', fn() => (new ArrayObject)->setIteratorClass('AoSerIt'));
aoShow('unknown class', fn() => (new ArrayObject)->__unserialize([0, [], [], 'NoSuchIterClass']));
aoShow('not an Iterator', fn() => (new ArrayObject)->__unserialize([0, [], [], 'stdClass']));
// php words both with `ArrayObject`, whichever class is the receiver.
aoShow('ArrayIterator says ArrayObject too',
    fn() => (new ArrayIterator)->__unserialize([0, [], [], 'stdClass']));

echo "-- ill-typed data\n";
aoShow('missing index 2', fn() => (new ArrayObject)->__unserialize([0, []]));
aoShow('index 3 is optional', function () {
    $a = new ArrayObject;
    $a->__unserialize([0, ['x' => 1], []]);
    return $a->getIteratorClass();
});
aoShow('flags is a string', fn() => (new ArrayObject)->__unserialize(['0', [], []]));
aoShow('members is a string', fn() => (new ArrayObject)->__unserialize([0, [], 'x']));
aoShow('iterator class is an int', fn() => (new ArrayObject)->__unserialize([0, [], [], 5]));
aoShow('storage is an int', fn() => (new ArrayObject)->__unserialize([0, 5, []]));
aoShow('not an array', fn() => (new ArrayObject)->__unserialize(5));

echo "-- php masks the flags word on the write\n";
aoShow('setFlags(-1)', function () { $a = new ArrayObject; $a->setFlags(-1); return (string)$a->getFlags(); });
aoShow('ctor flags', fn() => (string)(new ArrayObject([], -1))->getFlags());
aoShow('and the payload agrees', fn() => json_encode((new ArrayObject([], -1))->__serialize()[0]));
--EXPECT--
-- the payload is php's list
ArrayObject => O:11:"ArrayObject":4:{i:0;i:0;i:1;a:2:{i:0;i:1;s:1:"k";i:2;}i:2;a:0:{}i:3;N;}
ArrayIterator => O:13:"ArrayIterator":4:{i:0;i:0;i:1;a:2:{i:0;i:1;s:1:"k";i:2;}i:2;a:0:{}i:3;N;}
with flags => [2,{"k":1},[],null]
with an iterator class => [1,{"k":1},[],"RecursiveArrayIterator"]
subclass members => O:8:"AoSerKid":4:{i:0;i:0;i:1;a:1:{i:0;i:7;}i:2;a:1:{s:1:"p";i:1;}i:3;N;}
-- and it round-trips
ArrayObject => {"0":1,"k":2}
ArrayIterator => {"0":1,"k":2}
subclass => AoSerKid|9|[7]
iterator class survives => RecursiveArrayIterator
-- a payload PHP wrote is readable
php bytes => {"0":1,"k":2}
flags come back => 2|{"x":1}
-- the restore accepts any Iterator, the setter does not
restore a plain Iterator => AoSerIt
setIteratorClass refuses it => TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, AoSerIt given
unknown class => UnexpectedValueException: Cannot deserialize ArrayObject with iterator class 'NoSuchIterClass'; no such class exists
not an Iterator => UnexpectedValueException: Cannot deserialize ArrayObject with iterator class 'stdClass'; this class does not implement the Iterator interface
ArrayIterator says ArrayObject too => UnexpectedValueException: Cannot deserialize ArrayObject with iterator class 'stdClass'; this class does not implement the Iterator interface
-- ill-typed data
missing index 2 => UnexpectedValueException: Incomplete or ill-typed serialization data
index 3 is optional => ArrayIterator
flags is a string => UnexpectedValueException: Incomplete or ill-typed serialization data
members is a string => UnexpectedValueException: Incomplete or ill-typed serialization data
iterator class is an int => UnexpectedValueException: Incomplete or ill-typed serialization data
storage is an int => InvalidArgumentException: Passed variable is not an array or object
not an array => TypeError: ArrayObject::__unserialize(): Argument #1 ($data) must be of type array, int given
-- php masks the flags word on the write
setFlags(-1) => 65535
ctor flags => 65535
and the payload agrees => 65535
