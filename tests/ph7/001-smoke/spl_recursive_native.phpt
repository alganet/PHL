--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RecursiveArrayIterator carries CHILD_ARRAYS_ONLY into its children
--DESCRIPTION--
php's RecursiveArrayIterator reads the flag in BOTH recursion methods: an object
entry has no children when the iterator was told arrays only, and getChildren()
builds the child with the PARENT'S FLAGS, so the restriction survives every
level. The embedded PHP ignored the flag in both directions. php also answers
null rather than descending when nothing is current, and hands back an entry that
is already an instance of the called class instead of wrapping it again. The
interface half: `interface X extends Iterator` is a PARENT, so Iterator's five
methods report Iterator as their declaring class — a native interface that named
it in its implements list claimed them as its own.
--FILE--
<?php
function srnShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
class SrnPlain { public $a = 1; }
class SrnOdd extends RecursiveFilterIterator {
    public function accept(): bool { $c = $this->current(); return !is_int($c) || $c % 2 === 1; }
}

/* hasChildren() reads the flag; getChildren() carries it down. */
srnShow('has children', function () {
    $r = new RecursiveArrayIterator([1, [2], new SrnPlain, 'x']);
    $out = [];
    foreach ($r as $k => $v) { $out[] = $k . ':' . var_export($r->hasChildren(), true); }
    return $out;
});
srnShow('has children arrays only', function () {
    $r = new RecursiveArrayIterator([1, [2], new SrnPlain],
        RecursiveArrayIterator::CHILD_ARRAYS_ONLY);
    $out = [];
    foreach ($r as $k => $v) { $out[] = $k . ':' . var_export($r->hasChildren(), true); }
    return $out;
});
srnShow('children keep the flags', function () {
    $r = new RecursiveArrayIterator([[1]], RecursiveArrayIterator::CHILD_ARRAYS_ONLY);
    $r->rewind();
    return $r->getChildren()->getFlags();
});
srnShow('children of an array', function () {
    $r = new RecursiveArrayIterator([[1, 2]]);
    $r->rewind();
    $c = $r->getChildren();
    return [get_class($c), iterator_to_array($c), $c->getFlags()];
});
/* The object-children probe lives in spl_recursive_object_children.phpt: php 8.5
 * emits an E_DEPRECATED there ("Using an object as a backing array"), which is
 * loud under test-compat and would make this whole file fail against the oracle. */
srnShow('no object children under the flag', function () {
    $r = new RecursiveArrayIterator([new SrnPlain], RecursiveArrayIterator::CHILD_ARRAYS_ONLY);
    $r->rewind();
    return $r->getChildren();
});
srnShow('an entry of our own class passes through', function () {
    $inner = new RecursiveArrayIterator([9]);
    $r = new RecursiveArrayIterator([$inner]);
    $r->rewind();
    return $r->getChildren() === $inner;
});
srnShow('nothing current', function () {
    $r = new RecursiveArrayIterator([1]);
    $r->rewind(); $r->next();
    return [$r->valid(), $r->hasChildren(), $r->getChildren()];
});
srnShow('a scalar still descends', function () {
    $r = new RecursiveArrayIterator([5]);
    $r->rewind();
    return [get_class($r->getChildren()), iterator_to_array($r->getChildren())];
});
srnShow('the child is the called class', function () {
    $c = new class([[1]]) extends RecursiveArrayIterator {};
    $c->rewind();
    return get_class($c->getChildren()) === get_class($c);
});
srnShow('constant', fn() => RecursiveArrayIterator::CHILD_ARRAYS_ONLY);

/* RecursiveFilterIterator forwards both methods to its inner iterator. */
srnShow('filter', fn() => iterator_to_array(
    new SrnOdd(new RecursiveArrayIterator([1, 2, 3, [4, 5]]))));
srnShow('filter children', function () {
    $r = new SrnOdd(new RecursiveArrayIterator([[4, 5], 1]));
    $r->rewind();
    return [$r->hasChildren(), get_class($r->getChildren()),
        iterator_to_array($r->getChildren())];
});
srnShow('filter is abstract',
    fn() => new RecursiveFilterIterator(new RecursiveArrayIterator([1])));
srnShow('filter refuses a plain iterator', fn() => new SrnOdd(new ArrayIterator([1])));
srnShow('recursion still walks', fn() => iterator_to_array(
    new RecursiveIteratorIterator(new RecursiveArrayIterator([1, [2, [3]], 4])), false));
srnShow('recursion through the filter', fn() => iterator_to_array(
    new RecursiveIteratorIterator(new SrnOdd(new RecursiveArrayIterator([1, 2, [3, 4]]))), false));

/* An interface's parent DECLARES the methods it contributes. */
srnShow('inherited interface methods', function () {
    $out = [];
    foreach (['RecursiveIterator', 'OuterIterator', 'SeekableIterator'] as $i) {
        foreach ((new ReflectionClass($i))->getMethods() as $m) {
            $out[] = $i . '::' . $m->getName() . '=' . $m->getDeclaringClass()->getName();
        }
    }
    sort($out);
    return $out;
});
srnShow('hierarchy', fn() => [get_parent_class('RecursiveArrayIterator'),
    get_parent_class('RecursiveFilterIterator'),
    in_array('RecursiveIterator', class_implements('RecursiveArrayIterator'), true),
    in_array('Iterator', class_implements('RecursiveIterator'), true),
    (new ReflectionClass('RecursiveFilterIterator'))->isAbstract()]);
--EXPECT--
has children => array (  0 => '0:false',  1 => '1:true',  2 => '2:true',  3 => '3:false',)
has children arrays only => array (  0 => '0:false',  1 => '1:true',  2 => '2:false',)
children keep the flags => 4
children of an array => array (  0 => 'RecursiveArrayIterator',  1 =>   array (    0 => 1,    1 => 2,  ),  2 => 0,)
no object children under the flag => NULL
an entry of our own class passes through => true
nothing current => array (  0 => false,  1 => false,  2 => NULL,)
a scalar still descends => TypeError: ArrayIterator::__construct(): Argument #1 ($array) must be of type array, int given
the child is the called class => true
constant => 4
filter => array (  0 => 1,  2 => 3,  3 =>   array (    0 => 4,    1 => 5,  ),)
filter children => array (  0 => true,  1 => 'SrnOdd',  2 =>   array (    1 => 5,  ),)
filter is abstract => Error: Cannot instantiate abstract class RecursiveFilterIterator
filter refuses a plain iterator => TypeError: RecursiveFilterIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, ArrayIterator given
recursion still walks => array (  0 => 1,  1 => 2,  2 => 3,  3 => 4,)
recursion through the filter => array (  0 => 1,  1 => 3,)
inherited interface methods => array (  0 => 'OuterIterator::current=Iterator',  1 => 'OuterIterator::getInnerIterator=OuterIterator',  2 => 'OuterIterator::key=Iterator',  3 => 'OuterIterator::next=Iterator',  4 => 'OuterIterator::rewind=Iterator',  5 => 'OuterIterator::valid=Iterator',  6 => 'RecursiveIterator::current=Iterator',  7 => 'RecursiveIterator::getChildren=RecursiveIterator',  8 => 'RecursiveIterator::hasChildren=RecursiveIterator',  9 => 'RecursiveIterator::key=Iterator',  10 => 'RecursiveIterator::next=Iterator',  11 => 'RecursiveIterator::rewind=Iterator',  12 => 'RecursiveIterator::valid=Iterator',  13 => 'SeekableIterator::current=Iterator',  14 => 'SeekableIterator::key=Iterator',  15 => 'SeekableIterator::next=Iterator',  16 => 'SeekableIterator::rewind=Iterator',  17 => 'SeekableIterator::seek=SeekableIterator',  18 => 'SeekableIterator::valid=Iterator',)
hierarchy => array (  0 => 'ArrayIterator',  1 => 'FilterIterator',  2 => true,  3 => true,  4 => true,)
