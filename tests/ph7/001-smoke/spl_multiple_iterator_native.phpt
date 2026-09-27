--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
MultipleIterator steps several iterators in lockstep, in two modes
--DESCRIPTION--
php builds this class on SplObjectStorage — its struct IS one, which is why
__debugInfo() answers under SplObjectStorage's own mangled key — and adds two
flags. MIT_NEED_ALL is valid only while EVERY sub-iterator is, MIT_NEED_ANY while
any one is, and an empty set is never valid. current() and key() answer ARRAYS
built in attach order, and how they treat an exhausted member is the whole
difference between the modes: under NEED_ANY it contributes NULL, under NEED_ALL
it is `Called current() with non valid sub iterator` — a different refusal from
the empty set's `Called current() on an invalid iterator`. MIT_KEYS_ASSOC keys
that array by the $info each iterator was attached with, which is why a NULL info
raises at KEY time and a DUPLICATE one at attach; php compares infos with
identity, so the string "5" and the int 5 are different infos and both fit.
--FILE--
<?php
function mitShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function mitPair($flags) {
    $m = new MultipleIterator($flags);
    $m->attachIterator(new ArrayIterator(['a1', 'a2', 'a3']), 'A');
    $m->attachIterator(new ArrayIterator(['b1']), 'B');
    return $m;
}

mitShow('NEED_ALL walk', function () {
    $m = mitPair(MultipleIterator::MIT_NEED_ALL);
    $out = [];
    foreach ($m as $k => $v) { $out[] = json_encode($k) . '=' . json_encode($v); }
    return $out;
});
mitShow('NEED_ANY walk', function () {
    $m = mitPair(MultipleIterator::MIT_NEED_ANY);
    $out = [];
    foreach ($m as $k => $v) { $out[] = json_encode($k) . '=' . json_encode($v); }
    return $out;
});
mitShow('ASSOC walk', function () {
    $m = mitPair(MultipleIterator::MIT_NEED_ANY | MultipleIterator::MIT_KEYS_ASSOC);
    $out = [];
    foreach ($m as $k => $v) { $out[] = json_encode($k) . '=' . json_encode($v); }
    return $out;
});
/* The two refusals, and the one that is about the KEY rather than the iterator. */
mitShow('NEED_ALL past the end', function () {
    $m = mitPair(MultipleIterator::MIT_NEED_ALL);
    $m->rewind(); $m->next();
    return [$m->valid(), $m->current()];
});
mitShow('NEED_ALL key past the end', function () {
    $m = mitPair(MultipleIterator::MIT_NEED_ALL);
    $m->rewind(); $m->next();
    return $m->key();
});
mitShow('NEED_ANY all exhausted', function () {
    $m = mitPair(MultipleIterator::MIT_NEED_ANY);
    $m->rewind(); $m->next(); $m->next(); $m->next();
    return [$m->valid(), $m->current(), $m->key()];
});
mitShow('empty valid', fn() => (new MultipleIterator())->valid());
mitShow('empty current', fn() => (new MultipleIterator())->current());
mitShow('empty key', fn() => (new MultipleIterator())->key());
mitShow('ASSOC with a null info', function () {
    $m = new MultipleIterator(MultipleIterator::MIT_KEYS_ASSOC);
    $m->attachIterator(new ArrayIterator(['x']));
    $m->rewind();
    return $m->key();
});
/* An exhausted member is reported before the missing $info is. */
mitShow('ASSOC null info past the end', function () {
    $m = new MultipleIterator(MultipleIterator::MIT_KEYS_ASSOC | MultipleIterator::MIT_NEED_ALL);
    $m->attachIterator(new ArrayIterator(['x']));
    $m->rewind(); $m->next();
    return $m->key();
});
mitShow('ASSOC with an int info', function () {
    $m = new MultipleIterator(MultipleIterator::MIT_KEYS_ASSOC | MultipleIterator::MIT_NEED_ALL);
    $m->attachIterator(new ArrayIterator(['x']), 7);
    $m->rewind();
    return [$m->key(), $m->current()];
});

/* attach / detach / contains over the object table. */
mitShow('duplicate info', function () {
    $m = new MultipleIterator();
    $m->attachIterator(new ArrayIterator([1]), 'x');
    $m->attachIterator(new ArrayIterator([2]), 'x');
});
mitShow('"5" and 5 are different infos', function () {
    $m = new MultipleIterator();
    $m->attachIterator(new ArrayIterator([1]), '5');
    $m->attachIterator(new ArrayIterator([2]), 5);
    return $m->countIterators();
});
mitShow('true and 1 are the same info', function () {
    $m = new MultipleIterator();
    $m->attachIterator(new ArrayIterator([1]), true);
    $m->attachIterator(new ArrayIterator([2]), 1);
});
mitShow('a null info is never a duplicate', function () {
    $m = new MultipleIterator();
    $m->attachIterator(new ArrayIterator([1]));
    $m->attachIterator(new ArrayIterator([2]));
    return $m->countIterators();
});
mitShow('re-attaching one object replaces its info', function () {
    $m = new MultipleIterator(MultipleIterator::MIT_KEYS_ASSOC | MultipleIterator::MIT_NEED_ALL);
    $it = new ArrayIterator(['v']);
    $m->attachIterator($it, 'x');
    $m->attachIterator($it, 'y');
    $m->rewind();
    return [$m->countIterators(), $m->key()];
});
mitShow('detach and contains', function () {
    $m = new MultipleIterator();
    $it = new ArrayIterator([1]);
    $m->attachIterator($it);
    $out = [$m->containsIterator($it), $m->containsIterator(new ArrayIterator([1]))];
    $m->detachIterator($it);
    $m->detachIterator($it);           /* detaching an absent one is not an error */
    $out[] = $m->countIterators();
    return $out;
});
mitShow('attach refuses a non-Iterator', fn() => (new MultipleIterator())
    ->attachIterator(new ArrayObject([1])));
mitShow('an array info is refused', fn() => (new MultipleIterator())
    ->attachIterator(new ArrayIterator([1]), [1]));
mitShow('an object info is refused', fn() => (new MultipleIterator())
    ->attachIterator(new ArrayIterator([1]), new stdClass));

/* The flags word is stored as given, with no screen at all. */
mitShow('flags', function () {
    $m = new MultipleIterator();
    $out = [$m->getFlags()];
    foreach ([0, 3, 99, -1] as $f) { $m->setFlags($f); $out[] = $m->getFlags(); }
    foreach ([0, 2, 99, -1] as $f) { $out[] = (new MultipleIterator($f))->getFlags(); }
    return $out;
});
/* rewind() reaches every member; next() moves them all. */
mitShow('rewind reaches the members', function () {
    $m = new MultipleIterator();
    $it = new ArrayIterator([1, 2]);
    $it->next();
    $m->attachIterator($it);
    $m->rewind();
    return $m->current();
});
mitShow('presentation', fn() => [
    array_keys((new MultipleIterator())->__debugInfo()),
    (array) new MultipleIterator(),
    serialize(new MultipleIterator()),
    get_parent_class('MultipleIterator'),
]);
mitShow('not Countable', fn() => count(new MultipleIterator()));
mitShow('clone keeps the members', function () {
    $m = new MultipleIterator(MultipleIterator::MIT_NEED_ANY);
    $it = new ArrayIterator([1]);
    $m->attachIterator($it, 'k');
    $c = clone $m;
    $c->detachIterator($it);
    return [$m->countIterators(), $c->countIterators(), $c->getFlags()];
});
--EXPECT--
NEED_ALL walk => array (  0 => '[0,0]=["a1","b1"]',)
NEED_ANY walk => array (  0 => '[0,0]=["a1","b1"]',  1 => '[1,null]=["a2",null]',  2 => '[2,null]=["a3",null]',)
ASSOC walk => array (  0 => '{"A":0,"B":0}={"A":"a1","B":"b1"}',  1 => '{"A":1,"B":null}={"A":"a2","B":null}',  2 => '{"A":2,"B":null}={"A":"a3","B":null}',)
NEED_ALL past the end => RuntimeException: Called current() with non valid sub iterator
NEED_ALL key past the end => RuntimeException: Called key() with non valid sub iterator
NEED_ANY all exhausted => array (  0 => false,  1 =>   array (    0 => NULL,    1 => NULL,  ),  2 =>   array (    0 => NULL,    1 => NULL,  ),)
empty valid => false
empty current => RuntimeException: Called current() on an invalid iterator
empty key => RuntimeException: Called key() on an invalid iterator
ASSOC with a null info => InvalidArgumentException: Sub-Iterator is associated with NULL
ASSOC null info past the end => RuntimeException: Called key() with non valid sub iterator
ASSOC with an int info => array (  0 =>   array (    7 => 0,  ),  1 =>   array (    7 => 'x',  ),)
duplicate info => InvalidArgumentException: Key duplication error
"5" and 5 are different infos => 2
true and 1 are the same info => InvalidArgumentException: Key duplication error
a null info is never a duplicate => 2
re-attaching one object replaces its info => array (  0 => 1,  1 =>   array (    'y' => 0,  ),)
detach and contains => array (  0 => true,  1 => false,  2 => 0,)
attach refuses a non-Iterator => TypeError: MultipleIterator::attachIterator(): Argument #1 ($iterator) must be of type Iterator, ArrayObject given
an array info is refused => TypeError: MultipleIterator::attachIterator(): Argument #2 ($info) must be of type string|int|null, array given
an object info is refused => TypeError: MultipleIterator::attachIterator(): Argument #2 ($info) must be of type string|int|null, stdClass given
flags => array (  0 => 1,  1 => 0,  2 => 3,  3 => 99,  4 => -1,  5 => 0,  6 => 2,  7 => 99,  8 => -1,)
rewind reaches the members => array (  0 => 1,)
presentation => array (  0 =>   array (    0 => '' . "\0" . 'SplObjectStorage' . "\0" . 'storage',  ),  1 =>   array (  ),  2 => 'O:16:"MultipleIterator":0:{}',  3 => false,)
not Countable => TypeError: count(): Argument #1 ($value) must be of type Countable|array, MultipleIterator given
clone keeps the members => array (  0 => 1,  1 => 0,  2 => 0,)
