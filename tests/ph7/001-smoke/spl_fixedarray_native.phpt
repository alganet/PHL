--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SplFixedArray presents its own elements and iterates through an InternalIterator
--DESCRIPTION--
php PRESENTS this class as its elements: var_dump shows `object(SplFixedArray)#1
(3) { [0]=> … }`, the (array) cast yields them with their integer keys, and
serialize() writes them as INTEGER property names because __serialize() hands the
element array straight back. The embedded PHP exposed its private __a/__n on all
three surfaces and had none of the three serialization methods. Its getIterator()
also yielded a Generator where php answers an InternalIterator — one class name
wrong on a php-visible surface, and the reason two iterators over one array now
walk independently. php's offset rule is its own: an int, a bool and an
integer-like string are accepted, everything else is `Cannot access offset of type
%s on SplFixedArray`, and offsetExists RAISES for an undecodable offset rather
than answering false.
--FILE--
<?php
function sfaShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function sfaMake(array $v = [1, 2, 3]) {
    $f = new SplFixedArray(count($v));
    foreach ($v as $i => $x) { $f[$i] = $x; }
    return $f;
}

/* The presentation IS the elements, on every surface. */
sfaShow('cast yields the elements', fn() => (array)sfaMake());
sfaShow('serialize writes integer names', fn() => serialize(sfaMake()));
sfaShow('__serialize is the elements', fn() => sfaMake()->__serialize());
sfaShow('round trip', fn() => unserialize(serialize(sfaMake()))->toArray());
sfaShow('__wakeup survives', fn() => method_exists('SplFixedArray', '__wakeup'));
sfaShow('get_object_vars is empty', fn() => get_object_vars(sfaMake()));
sfaShow('no declared properties', fn() => (new ReflectionClass('SplFixedArray'))->getProperties());

/* getIterator() answers php's InternalIterator, and the cursors are independent. */
sfaShow('the iterator class', fn() => get_class(sfaMake()->getIterator()));
sfaShow('foreach', function () {
    $o = [];
    foreach (sfaMake() as $k => $v) { $o[] = "$k=$v"; }
    return implode(' ', $o);
});
sfaShow('holes are visited as null', function () {
    $f = new SplFixedArray(3);
    $f[1] = 'x';
    $o = [];
    foreach ($f as $k => $v) { $o[] = "$k=" . var_export($v, true); }
    return implode(' ', $o);
});
sfaShow('nested foreach', function () {
    $f = sfaMake([1, 2]);
    $o = [];
    foreach ($f as $x) { foreach ($f as $y) { $o[] = "$x$y"; } }
    return implode(' ', $o);
});
sfaShow('two iterators are independent', function () {
    $f = sfaMake();
    $i = $f->getIterator();
    $i->rewind();
    $i->next();
    $j = $f->getIterator();
    $j->rewind();
    return [$i->key(), $j->key()];
});

/* php's offset rule. */
sfaShow('int offset', fn() => sfaMake()[1]);
sfaShow('bool offset is 0/1', fn() => [sfaMake()[true], sfaMake()[false]]);
sfaShow('integer-like string offset', fn() => sfaMake()['1']);
sfaShow('a non-numeric string is refused', fn() => sfaMake()['x']);
sfaShow('null is refused', fn() => sfaMake()[null]);
sfaShow('an array is refused', fn() => sfaMake()[[1]]);
sfaShow('out of range', fn() => sfaMake()[9]);
sfaShow('negative', fn() => sfaMake()[-1]);
sfaShow('offsetSet out of range', fn() => sfaMake()->offsetSet(9, 'x'));
sfaShow('offsetUnset leaves a null in place', function () {
    $f = sfaMake();
    unset($f[1]);
    return [$f->toArray(), count($f)];
});
/* offsetExists raises for an undecodable offset and answers false for a null slot. */
sfaShow('offsetExists', function () {
    $f = new SplFixedArray(3);
    $f[0] = 'a';
    $f[2] = 'c';
    return [$f->offsetExists(0), $f->offsetExists(1), $f->offsetExists(9),
        $f->offsetExists(-1), $f->offsetExists('0')];
});
sfaShow('offsetExists refuses a bad string', fn() => sfaMake()->offsetExists('x'));
sfaShow('offsetExists refuses null', fn() => sfaMake()->offsetExists(null));

/* Sizing, and php's `true` return. */
sfaShow('default size', fn() => [(new SplFixedArray())->getSize(), count(new SplFixedArray())]);
sfaShow('a fresh array is all nulls', fn() => (new SplFixedArray(3))->toArray());
sfaShow('setSize answers true', fn() => sfaMake()->setSize(5));
sfaShow('grow pads with null', function () { $f = sfaMake(); $f->setSize(5); return $f->toArray(); });
sfaShow('shrink drops the tail', function () { $f = sfaMake(); $f->setSize(2); return $f->toArray(); });
sfaShow('setSize(0) empties', function () { $f = sfaMake(); $f->setSize(0); return [$f->toArray(), count($f)]; });
sfaShow('a negative size is refused', fn() => sfaMake()->setSize(-1));
sfaShow('the constructor words its own refusal', fn() => new SplFixedArray(-1));
sfaShow('the constructor refuses a non-numeric string', fn() => new SplFixedArray('zz'));
sfaShow('setSize refuses a non-numeric string', fn() => sfaMake()->setSize('zz'));

/* fromArray. */
sfaShow('fromArray list', fn() => SplFixedArray::fromArray([1, 2, 3])->toArray());
sfaShow('fromArray keeps the gaps', fn() => SplFixedArray::fromArray([0 => 'a', 3 => 'b'])->toArray());
sfaShow('fromArray can drop the keys', fn() => SplFixedArray::fromArray([0 => 'a', 3 => 'b'], false)->toArray());
sfaShow('fromArray refuses string keys', fn() => SplFixedArray::fromArray(['a' => 1]));
sfaShow('fromArray ignores them when told to', fn() => SplFixedArray::fromArray(['a' => 1], false)->toArray());
sfaShow('fromArray refuses a negative key', fn() => SplFixedArray::fromArray([-1 => 'a']));
sfaShow('fromArray refuses a non-array', fn() => SplFixedArray::fromArray('x'));
sfaShow('fromArray empty', fn() => [SplFixedArray::fromArray([])->toArray(),
    SplFixedArray::fromArray([])->getSize()]);

sfaShow('json', fn() => json_encode(sfaMake()));
sfaShow('clone is independent', function () {
    $f = sfaMake();
    $c = clone $f;
    $c[0] = 'z';
    return [$f[0], $c[0]];
});
--EXPECT--
cast yields the elements => array (  0 => 1,  1 => 2,  2 => 3,)
serialize writes integer names => 'O:13:"SplFixedArray":3:{i:0;i:1;i:1;i:2;i:2;i:3;}'
__serialize is the elements => array (  0 => 1,  1 => 2,  2 => 3,)
round trip => array (  0 => 1,  1 => 2,  2 => 3,)
__wakeup survives => true
get_object_vars is empty => array ()
no declared properties => array ()
the iterator class => 'InternalIterator'
foreach => '0=1 1=2 2=3'
holes are visited as null => '0=NULL 1=\'x\' 2=NULL'
nested foreach => '11 12 21 22'
two iterators are independent => array (  0 => 1,  1 => 0,)
int offset => 2
bool offset is 0/1 => array (  0 => 2,  1 => 1,)
integer-like string offset => 2
a non-numeric string is refused => TypeError: Cannot access offset of type string on SplFixedArray
null is refused => TypeError: Cannot access offset of type null on SplFixedArray
an array is refused => TypeError: Cannot access offset of type array on SplFixedArray
out of range => OutOfBoundsException: Index invalid or out of range
negative => OutOfBoundsException: Index invalid or out of range
offsetSet out of range => OutOfBoundsException: Index invalid or out of range
offsetUnset leaves a null in place => array (  0 =>   array (    0 => 1,    1 => NULL,    2 => 3,  ),  1 => 3,)
offsetExists => array (  0 => true,  1 => false,  2 => false,  3 => false,  4 => true,)
offsetExists refuses a bad string => TypeError: Cannot access offset of type string on SplFixedArray
offsetExists refuses null => TypeError: Cannot access offset of type null on SplFixedArray
default size => array (  0 => 0,  1 => 0,)
a fresh array is all nulls => array (  0 => NULL,  1 => NULL,  2 => NULL,)
setSize answers true => true
grow pads with null => array (  0 => 1,  1 => 2,  2 => 3,  3 => NULL,  4 => NULL,)
shrink drops the tail => array (  0 => 1,  1 => 2,)
setSize(0) empties => array (  0 =>   array (  ),  1 => 0,)
a negative size is refused => ValueError: SplFixedArray::setSize(): Argument #1 ($size) must be greater than or equal to 0
the constructor words its own refusal => ValueError: SplFixedArray::__construct(): Argument #1 ($size) must be greater than or equal to 0
the constructor refuses a non-numeric string => TypeError: SplFixedArray::__construct(): Argument #1 ($size) must be of type int, string given
setSize refuses a non-numeric string => TypeError: SplFixedArray::setSize(): Argument #1 ($size) must be of type int, string given
fromArray list => array (  0 => 1,  1 => 2,  2 => 3,)
fromArray keeps the gaps => array (  0 => 'a',  1 => NULL,  2 => NULL,  3 => 'b',)
fromArray can drop the keys => array (  0 => 'a',  1 => 'b',)
fromArray refuses string keys => InvalidArgumentException: array must contain only positive integer keys
fromArray ignores them when told to => array (  0 => 1,)
fromArray refuses a negative key => InvalidArgumentException: array must contain only positive integer keys
fromArray refuses a non-array => TypeError: SplFixedArray::fromArray(): Argument #1 ($array) must be of type array, string given
fromArray empty => array (  0 =>   array (  ),  1 => 0,)
json => '[1,2,3]'
clone is independent => array (  0 => 1,  1 => 'z',)
