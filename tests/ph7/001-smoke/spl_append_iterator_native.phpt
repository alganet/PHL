--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
AppendIterator keeps its list in a real ArrayIterator, and that list is its cursor
--DESCRIPTION--
php holds the appended iterators in an actual ArrayIterator instance — the object
getArrayIterator() hands out — and walks it with a cursor over the SAME storage.
Both halves are php-visible: appending THROUGH the returned object feeds the walk,
rewinding it restarts the walk, and once the cursor runs off the end
getIteratorIndex() answers null. The embedded PHP kept a private array and answered
getArrayIterator() with a fresh iterator over a COPY, so none of that worked and
append() after exhaustion could not resume. Resuming also needs php's ArrayIterator
rule that an insertion revives a cursor which ran past the end — php's position is
an integer index, so it sits at the element count rather than nowhere.
--FILE--
<?php
function sainShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function sainAppend(array $lists) {
    $a = new AppendIterator();
    foreach ($lists as $l) { $a->append(new ArrayIterator($l)); }
    return $a;
}

sainShow('iterate', fn() => iterator_to_array(sainAppend([[1, 2], ['x' => 3]])));
sainShow('iterate values', fn() => iterator_to_array(sainAppend([[1, 2], [3]]), false));
sainShow('empty inners skipped', function () {
    $a = sainAppend([[], [], [7]]);
    return [iterator_to_array($a), $a->getIteratorIndex()];
});

/* append() itself fetches, so a fresh AppendIterator is live before any rewind. */
sainShow('live after append', function () {
    $a = sainAppend([[1, 2]]);
    return [$a->valid(), $a->current(), $a->key(), $a->getIteratorIndex(),
        get_class($a->getInnerIterator())];
});
sainShow('nothing appended', function () {
    $a = new AppendIterator();
    return [$a->valid(), $a->current(), $a->key(), $a->getIteratorIndex(),
        $a->getInnerIterator()];
});
/* The index walks the LIST, and runs off its end with the walk. */
sainShow('index walk', function () {
    $a = sainAppend([[1], [2]]);
    $out = [];
    for ($a->rewind(); $a->valid(); $a->next()) {
        $out[] = $a->getIteratorIndex() . ':' . $a->key() . '=' . $a->current();
    }
    $out[] = var_export($a->getIteratorIndex(), true) . '/' . var_export($a->valid(), true);
    return $out;
});
sainShow('all empty', function () {
    $a = sainAppend([[]]);
    return [iterator_to_array($a), $a->valid(), $a->getIteratorIndex()];
});

/* getArrayIterator() is the SAME object every time, and it is the live list. */
sainShow('list identity', function () {
    $a = new AppendIterator();
    $l = $a->getArrayIterator();
    $was = [get_class($l), $l === $a->getArrayIterator(), count($l)];
    $a->append(new ArrayIterator([1]));
    return array_merge($was, [count($l)]);
});
sainShow('append through the list', function () {
    $a = new AppendIterator();
    $a->getArrayIterator()->append(new ArrayIterator([5, 6]));
    return iterator_to_array($a, false);
});
sainShow('the list cursor is the index', function () {
    $a = sainAppend([[1], [2]]);
    $l = $a->getArrayIterator();
    $seen = [];
    foreach ($a as $v) { $seen[] = $v . '@' . $l->key(); }
    $seen[] = var_export($l->key(), true);
    $l->rewind();
    $seen[] = $a->getIteratorIndex() . '/' . $a->current();
    return $seen;
});
sainShow('seeking the list moves the index', function () {
    $a = sainAppend([[1], [2]]);
    $a->getArrayIterator()->seek(1);
    return [$a->getIteratorIndex(), $a->current()];
});

/* append() mid-walk and after exhaustion. */
sainShow('append while iterating', function () {
    $a = sainAppend([[1]]);
    $out = [];
    foreach ($a as $v) {
        $out[] = $v;
        if (count($out) === 1) { $a->append(new ArrayIterator([9])); }
    }
    return $out;
});
sainShow('append after exhaustion', function () {
    $a = sainAppend([[1]]);
    foreach ($a as $v) {}
    $was = $a->valid();
    $a->append(new ArrayIterator([2]));
    return [$was, $a->valid(), $a->current(), $a->getIteratorIndex()];
});
/* The ArrayIterator rule the resumption rests on. */
sainShow('exhausted cursor revives', function () {
    $i = new ArrayIterator([1]);
    $i->next(); $i->next();
    $was = $i->valid();
    $i->append(2);
    return [$was, $i->valid(), $i->key(), $i->current()];
});
sainShow('new key revives too', function () {
    $i = new ArrayIterator([1]);
    $i->next();
    $i['x'] = 5;
    return [$i->valid(), $i->key(), $i->current()];
});
sainShow('an overwrite does not', function () {
    $i = new ArrayIterator([1]);
    $i->next();
    $i[0] = 9;
    return [$i->valid(), $i->key(), $i->current()];
});

/* current() re-fetches, unlike every other decorator's cached one. */
sainShow('follows a moved inner', function () {
    $i = new ArrayIterator([1, 2, 3]);
    $a = new AppendIterator();
    $a->append($i);
    $a->rewind();
    $i->next();
    return [$a->current(), $a->key(), $a->valid()];
});
sainShow('inner identity', function () {
    $i = new ArrayIterator([1]);
    $a = new AppendIterator();
    $a->append($i);
    return $a->getInnerIterator() === $i;
});
sainShow('nested', function () {
    $inner = sainAppend([[1, 2]]);
    $a = new AppendIterator();
    $a->append($inner);
    return iterator_to_array($a, false);
});

/* The declaration php writes. */
sainShow('append type', fn() => (new AppendIterator())->append(new ArrayObject([1])));
sainShow('append array', fn() => (new AppendIterator())->append([1]));
sainShow('ctor arity', fn() => new AppendIterator(new ArrayIterator([1])));
sainShow('uncloneable', fn() => clone new AppendIterator());
sainShow('properties', fn() => [
    array_map(fn($p) => $p->getName(), (new ReflectionClass('AppendIterator'))->getProperties()),
    get_object_vars(sainAppend([[1]]))]);
sainShow('hierarchy', fn() => [get_parent_class('AppendIterator'),
    (new ReflectionClass('AppendIterator'))->isInternal()]);
--EXPECT--
iterate => array (  0 => 1,  1 => 2,  'x' => 3,)
iterate values => array (  0 => 1,  1 => 2,  2 => 3,)
empty inners skipped => array (  0 =>   array (    0 => 7,  ),  1 => NULL,)
live after append => array (  0 => true,  1 => 1,  2 => 0,  3 => 0,  4 => 'ArrayIterator',)
nothing appended => array (  0 => false,  1 => NULL,  2 => NULL,  3 => NULL,  4 => NULL,)
index walk => array (  0 => '0:0=1',  1 => '1:0=2',  2 => 'NULL/false',)
all empty => array (  0 =>   array (  ),  1 => false,  2 => NULL,)
list identity => array (  0 => 'ArrayIterator',  1 => true,  2 => 0,  3 => 1,)
append through the list => array (  0 => 5,  1 => 6,)
the list cursor is the index => array (  0 => '1@0',  1 => '2@1',  2 => 'NULL',  3 => '0/',)
seeking the list moves the index => array (  0 => 1,  1 => 1,)
append while iterating => array (  0 => 1,  1 => 9,)
append after exhaustion => array (  0 => false,  1 => true,  2 => 2,  3 => 1,)
exhausted cursor revives => array (  0 => false,  1 => true,  2 => 1,  3 => 2,)
new key revives too => array (  0 => true,  1 => 'x',  2 => 5,)
an overwrite does not => array (  0 => false,  1 => NULL,  2 => NULL,)
follows a moved inner => array (  0 => 2,  1 => 1,  2 => true,)
inner identity => true
nested => array (  0 => 1,  1 => 2,)
append type => TypeError: AppendIterator::append(): Argument #1 ($iterator) must be of type Iterator, ArrayObject given
append array => TypeError: AppendIterator::append(): Argument #1 ($iterator) must be of type Iterator, array given
ctor arity => ArgumentCountError: AppendIterator::__construct() expects exactly 0 arguments, 1 given
uncloneable => Error: Trying to clone an uncloneable object of class AppendIterator
properties => array (  0 =>   array (  ),  1 =>   array (  ),)
hierarchy => array (  0 => 'IteratorIterator',  1 => true,)
