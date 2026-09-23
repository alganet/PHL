--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayIterator and ArrayObject are C classes, and the PHL-only trait is gone
--DESCRIPTION--
The two shared one implementation through `trait __SplStoreT`, the last PHL-only
trait and the last §4 name that was not a function. php shares nothing between
them at the type level, so the C version does not either: one set of bodies,
named by both classes, no common parent and the exact interface list php has.
Converting DECLARED the parameters — `object|array $array`, `int $flags`,
`callable $callback` — which is also how `asort(SORT_STRING)` started working:
the PHP took a `$flags` parameter and then dropped it on the floor. A subclass
still extends the native class, overrides its methods and reaches `parent::`.
--FILE--
<?php
function sasnShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
/* The trait is not a declared type any more. */
sasnShow('trait gone', fn() => [trait_exists('__SplStoreT'), class_exists('__SplStoreT')]);

/* The store, through both classes. */
sasnShow('copy', fn() => (new ArrayIterator(['a' => 1, 5 => 'five']))->getArrayCopy());
sasnShow('count', fn() => [count(new ArrayIterator([1, 2])), count(new ArrayObject([1, 2, 3]))]);
sasnShow('offsets', function () {
    $i = new ArrayIterator([1]);
    $i[] = 2; $i['k'] = 3; unset($i[0]);
    return [$i->getArrayCopy(), isset($i['k']), isset($i['nope'])];
});
sasnShow('append', function () { $a = new ArrayObject([]); $a->append('x'); return $a->getArrayCopy(); });

/* Sorting honours the $flags the PHP declared and ignored. */
sasnShow('asort flags', function () {
    $i = new ArrayIterator(['a' => 10, 'b' => 9, 'c' => 100]);
    $i->asort(SORT_STRING);
    return $i->getArrayCopy();
});
sasnShow('asort default', function () {
    $i = new ArrayIterator(['a' => 10, 'b' => 9, 'c' => 100]);
    $i->asort();
    return $i->getArrayCopy();
});
sasnShow('ksort flags', function () {
    $i = new ArrayIterator(['10' => 1, '9' => 2, '100' => 3]);
    $i->ksort(SORT_STRING);
    return array_keys($i->getArrayCopy());
});
sasnShow('natsort', function () { $i = new ArrayIterator(['img12', 'img10', 'img2']); $i->natsort(); return $i->getArrayCopy(); });
sasnShow('natcasesort', function () { $i = new ArrayIterator(['IMG12', 'img10', 'IMG2']); $i->natcasesort(); return $i->getArrayCopy(); });
sasnShow('uasort', function () { $i = new ArrayIterator(['b' => 2, 'a' => 1]); $i->uasort(fn($x, $y) => $x <=> $y); return $i->getArrayCopy(); });
sasnShow('uksort', function () { $i = new ArrayIterator(['b' => 2, 'a' => 1]); $i->uksort(fn($x, $y) => strcmp($x, $y)); return $i->getArrayCopy(); });

/* The cursor. */
sasnShow('seek', function () { $i = new ArrayIterator(['a' => 1, 'b' => 2, 'c' => 3]); $i->seek(2); return [$i->key(), $i->current(), $i->valid()]; });
sasnShow('past end', function () { $i = new ArrayIterator([1]); $i->rewind(); $i->next(); return [$i->valid(), $i->current(), $i->key()]; });
sasnShow('iterate', fn() => iterator_to_array(new ArrayIterator(['x' => 1, 'y' => 2])));

/* ArrayObject's iterator class and ARRAY_AS_PROPS. */
sasnShow('iterator class', function () {
    $a = new ArrayObject([1, 2], 0, 'RecursiveArrayIterator');
    return [$a->getIteratorClass(), get_class($a->getIterator()), iterator_to_array($a->getIterator())];
});
sasnShow('props', function () {
    $a = new ArrayObject(['k' => 1], ArrayObject::ARRAY_AS_PROPS);
    $a->j = 2;
    return [$a->k, isset($a->j), isset($a->nope), $a->getArrayCopy()];
});
sasnShow('exchange', function () { $a = new ArrayObject(['a' => 1]); $old = $a->exchangeArray(['b' => 2]); return [$old, $a->getArrayCopy()]; });
sasnShow('clone', function () { $a = new ArrayObject(['a' => 1]); $b = clone $a; $b['a'] = 2; return [$a['a'], $b['a']]; });

/* Declared parameters: named arguments, and php's refusals. */
sasnShow('named ctor', fn() => (new ArrayObject(flags: ArrayObject::ARRAY_AS_PROPS))->getFlags());
sasnShow('named ctor 2', fn() => (new ArrayIterator(flags: 1, array: ['x' => 1]))->getArrayCopy());
sasnShow('ctor type', fn() => new ArrayIterator('nope'));
sasnShow('ctor null', fn() => new ArrayObject(null));
sasnShow('too many', fn() => (new ArrayIterator([]))->count(1));
sasnShow('seek oob', fn() => (new ArrayIterator([1]))->seek(5));
sasnShow('bad iterator class', fn() => (new ArrayObject([]))->setIteratorClass('stdClass'));

/* Subclassing a native class: RecursiveArrayIterator is still PHP, and a user one
 * overrides a native method and calls parent::. */
class SasnKid extends ArrayIterator {
    public function current(): mixed { return '!' . parent::current(); }
}
sasnShow('RAI', fn() => [get_parent_class('RecursiveArrayIterator'), (new RecursiveArrayIterator([1]))->count()]);
sasnShow('RAI children', function () {
    $r = new RecursiveArrayIterator([[2, 3]]);
    $r->rewind();
    return [$r->hasChildren(), get_class($r->getChildren()), $r->getChildren()->getArrayCopy()];
});
sasnShow('override', function () { $i = new SasnKid([1, 2]); $i->rewind(); return $i->current(); });
sasnShow('anon subclass', function () {
    $c = new class(['a' => 1]) extends ArrayObject { public function extra() { return count($this) + 10; } };
    return [$c->extra(), $c['a']];
});
sasnShow('instanceof', fn() => [
    (new ArrayIterator([])) instanceof SeekableIterator,
    (new ArrayIterator([])) instanceof Iterator,
    (new ArrayObject([])) instanceof IteratorAggregate,
    (new ArrayObject([])) instanceof ArrayAccess,
]);
echo "end\n";
?>
--EXPECT--
trait gone => array (  0 => false,  1 => false,)
copy => array (  'a' => 1,  5 => 'five',)
count => array (  0 => 2,  1 => 3,)
offsets => array (  0 =>   array (    1 => 2,    'k' => 3,  ),  1 => true,  2 => false,)
append => array (  0 => 'x',)
asort flags => array (  'a' => 10,  'c' => 100,  'b' => 9,)
asort default => array (  'b' => 9,  'a' => 10,  'c' => 100,)
ksort flags => array (  0 => 10,  1 => 100,  2 => 9,)
natsort => array (  2 => 'img2',  1 => 'img10',  0 => 'img12',)
natcasesort => array (  2 => 'IMG2',  1 => 'img10',  0 => 'IMG12',)
uasort => array (  'a' => 1,  'b' => 2,)
uksort => array (  'a' => 1,  'b' => 2,)
seek => array (  0 => 'c',  1 => 3,  2 => true,)
past end => array (  0 => false,  1 => NULL,  2 => NULL,)
iterate => array (  'x' => 1,  'y' => 2,)
iterator class => array (  0 => 'RecursiveArrayIterator',  1 => 'RecursiveArrayIterator',  2 =>   array (    0 => 1,    1 => 2,  ),)
props => array (  0 => 1,  1 => true,  2 => false,  3 =>   array (    'k' => 1,    'j' => 2,  ),)
exchange => array (  0 =>   array (    'a' => 1,  ),  1 =>   array (    'b' => 2,  ),)
clone => array (  0 => 1,  1 => 2,)
named ctor => 2
named ctor 2 => array (  'x' => 1,)
ctor type => TypeError: ArrayIterator::__construct(): Argument #1 ($array) must be of type array, string given
ctor null => TypeError: ArrayObject::__construct(): Argument #1 ($array) must be of type array, null given
too many => ArgumentCountError: ArrayIterator::count() expects exactly 0 arguments, 1 given
seek oob => OutOfBoundsException: Seek position 5 is out of range
bad iterator class => TypeError: ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be a class name derived from ArrayIterator, stdClass given
RAI => array (  0 => 'ArrayIterator',  1 => 1,)
RAI children => array (  0 => true,  1 => 'RecursiveArrayIterator',  2 =>   array (    0 => 2,    1 => 3,  ),)
override => '!1'
anon subclass => array (  0 => 11,  1 => 1,)
instanceof => array (  0 => true,  1 => true,  2 => true,  3 => true,)
end
