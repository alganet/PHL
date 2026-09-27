--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CachingIterator is one step ahead of its inner iterator, and its twin
--DESCRIPTION--
Every fetch copies current()/key() into the cache and then ADVANCES the inner
iterator, which is the whole design: hasNext() is the inner's live valid(), asked
after that step, and moving the inner behind the decorator's back changes the
answer. The rest of the class happens inside that same fetch, in php's order —
the FULL_CACHE entry, then the recursive twin's CHILDREN, then the string form,
then the step. That string is eager: CALL_TOSTRING casts the ELEMENT and
TOSTRING_USE_INNER casts the inner ITERATOR at FETCH time, so the default
instance over arrays warns once per element and over unstringable objects throws
from rewind(); TOSTRING_USE_KEY / TOSTRING_USE_CURRENT read the cache at
__toString() time instead, and no spelling at all is a BadMethodCallException
naming the RECEIVER's class. getFlags() answers the raw word with php's private
CIT_VALID (0x10000) in it, the CONSTRUCTOR masks with 0xFFFF where setFlags()
keeps the high half, and setFlags() refuses to unset either eager spelling.
--FILE--
<?php
/* Warnings print the FILE they came from, so the two this test provokes go
 * through a handler instead — restored at the end, since every 001-smoke test
 * shares one interpreter. */
set_error_handler(function ($no, $msg) {
    if (error_reporting() === 0) { return false; }
    echo 'WARN: ', $msg, "\n";
    return true;
});
function citShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
$citFlat = ['x' => 1, 'y' => 2, 'z' => 3];
$citTree = ['a' => 1, 'b' => ['c' => 2], 'd' => 3];

/* The walk, with hasNext() and the eager string at each step. */
citShow('walk', function () {
    $it = new CachingIterator(new ArrayIterator($GLOBALS['citFlat']));
    $out = [];
    foreach ($it as $k => $v) { $out[] = "$k=$v/" . var_export($it->hasNext(), true) . "/$it"; }
    return $out;
});
/* hasNext() is the INNER iterator's live valid(), not a flag taken at the fetch. */
citShow('hasNext is live', function () {
    $inner = new ArrayIterator(['a', 'b', 'c']);
    $it = new CachingIterator($inner);
    $it->rewind();
    $before = $it->hasNext();
    $inner->next(); $inner->next();
    return [$before, $it->hasNext(), $it->current(), $inner->key()];
});
citShow('hasNext over an empty inner', fn() => (new CachingIterator(new ArrayIterator([])))->hasNext());
citShow('past the end', function () {
    $it = new CachingIterator(new ArrayIterator(['a']));
    $it->rewind(); $it->next();
    return [$it->valid(), $it->getFlags(), (string) $it, $it->key(), $it->current()];
});

/* getFlags() answers the raw word: CIT_VALID rides in it while a fetch stands. */
citShow('valid bit', function () {
    $it = new CachingIterator(new ArrayIterator(['a']), CachingIterator::TOSTRING_USE_KEY);
    $before = $it->getFlags();
    $it->rewind();
    $mid = $it->getFlags();
    $it->next();
    return [$before, $mid, $it->getFlags()];
});
/* The constructor masks with 0xFFFF; setFlags() keeps what the mask dropped. */
citShow('ctor keeps 1024', fn() => (new CachingIterator(new ArrayIterator([1]), 1024))->getFlags());
citShow('ctor masks 0x10000', fn() => (new CachingIterator(new ArrayIterator([1]), 0x10001))->getFlags());
citShow('setFlags keeps the high half', function () {
    $it = new CachingIterator(new ArrayIterator([1]), 1024 | 1);
    $it->setFlags(1 | 256 | 16);
    return $it->getFlags();
});
citShow('two spellings at once', fn() => new CachingIterator(new ArrayIterator([1]),
    CachingIterator::CALL_TOSTRING | CachingIterator::TOSTRING_USE_KEY));
citShow('setFlags two at once', function () {
    $it = new CachingIterator(new ArrayIterator([1]));
    $it->setFlags(CachingIterator::CALL_TOSTRING | CachingIterator::TOSTRING_USE_KEY);
});
citShow('unsetting CALL_TOSTRING', function () {
    $it = new CachingIterator(new ArrayIterator([1]));
    $it->setFlags(CachingIterator::FULL_CACHE);
});
citShow('unsetting TOSTRING_USE_INNER', function () {
    $it = new CachingIterator(new ArrayIterator([1]), CachingIterator::TOSTRING_USE_INNER);
    $it->setFlags(CachingIterator::TOSTRING_USE_KEY);
});
citShow('a refused setFlags changes nothing', function () {
    $it = new CachingIterator(new ArrayIterator([1]));
    try { $it->setFlags(0); } catch (Throwable $e) {}
    return $it->getFlags();
});

/* The four string spellings. */
citShow('use key', function () {
    $it = new CachingIterator(new ArrayIterator(['k' => 'v']), CachingIterator::TOSTRING_USE_KEY);
    $it->rewind();
    return (string) $it;
});
citShow('use current', function () {
    $it = new CachingIterator(new ArrayIterator(['k' => 'v']), CachingIterator::TOSTRING_USE_CURRENT);
    $it->rewind();
    return (string) $it;
});
class CitInnerString extends ArrayIterator {
    public function __toString(): string { return 'INNER'; }
}
citShow('use inner', function () {
    $it = new CachingIterator(new CitInnerString(['a']), CachingIterator::TOSTRING_USE_INNER);
    $it->rewind();
    return (string) $it;
});
citShow('no spelling', function () {
    $it = new CachingIterator(new ArrayIterator(['a']), 0);
    $it->rewind();
    return (string) $it;
});
class CitSub extends CachingIterator {}
citShow('the refusal names the receiver', function () {
    $it = new CitSub(new ArrayIterator(['a']), 0);
    return (string) $it;
});
/* The eager cast happens at the FETCH, not at the (string). */
citShow('cast is eager', function () {
    $it = new CachingIterator(new ArrayIterator([new stdClass]));
    $it->rewind();
});
citShow('array element casts loudly', function () {
    $it = new CachingIterator(new ArrayIterator([['nested']]), CachingIterator::CALL_TOSTRING);
    $it->rewind();
    return (string) $it;
});

/* FULL_CACHE, and the four ArrayAccess members over it. */
citShow('cache fills as it walks', function () {
    $it = new CachingIterator(new ArrayIterator($GLOBALS['citFlat']),
        CachingIterator::FULL_CACHE | CachingIterator::TOSTRING_USE_KEY);
    $out = [];
    $it->rewind();
    while ($it->valid()) { $out[] = count($it) . ':' . implode(',', array_keys($it->getCache())); $it->next(); }
    $out[] = 'end:' . implode(',', array_keys($it->getCache()));
    $it->rewind();
    $out[] = 'again:' . implode(',', array_keys($it->getCache()));
    return $out;
});
citShow('array access', function () {
    $it = new CachingIterator(new ArrayIterator($GLOBALS['citFlat']),
        CachingIterator::FULL_CACHE | CachingIterator::TOSTRING_USE_KEY);
    foreach ($it as $v) {}
    $it['q'] = 9;
    $out = [$it['x'], isset($it['x']), isset($it['nope']), $it['q'], count($it)];
    unset($it['q']);
    $out[] = count($it);
    return $out;
});
citShow('a missing key is quoted whatever it was', function () {
    $it = new CachingIterator(new ArrayIterator(['x' => 1]),
        CachingIterator::FULL_CACHE | CachingIterator::TOSTRING_USE_KEY);
    foreach ($it as $v) {}
    return [$it[0], $it[true], $it[1.9]];
});
citShow('an array key is refused', function () {
    $it = new CachingIterator(new ArrayIterator(['x' => 1]),
        CachingIterator::FULL_CACHE | CachingIterator::TOSTRING_USE_KEY);
    return $it[[1]];
});
foreach (['getCache', 'count', 'offsetGet', 'offsetExists', 'offsetSet', 'offsetUnset'] as $citM) {
    citShow("no cache: $citM", function () use ($citM) {
        $it = new CachingIterator(new ArrayIterator([1]));
        return $citM === 'offsetSet' ? $it->offsetSet('k', 1)
            : ($citM === 'count' ? count($it)
            : ($citM === 'getCache' ? $it->getCache() : $it->$citM('k')));
    });
}

/* The recursive twin: children are built at the FETCH and handed back as one
 * object; the flags go down masked, and the child is a plain
 * RecursiveCachingIterator even under a subclass. */
citShow('children', function () {
    $it = new RecursiveCachingIterator(new RecursiveArrayIterator($GLOBALS['citTree']),
        RecursiveCachingIterator::TOSTRING_USE_KEY);
    $out = [];
    $it->rewind();
    while ($it->valid()) {
        $kids = $it->getChildren();
        $out[] = $it->key() . '/' . var_export($it->hasChildren(), true)
            . '/' . ($kids === null ? 'null' : get_class($kids) . ':' . $kids->getFlags());
        $it->next();
    }
    $out[] = 'end/' . var_export($it->hasChildren(), true);
    return $out;
});
citShow('the same child object twice', function () {
    $it = new RecursiveCachingIterator(new RecursiveArrayIterator($GLOBALS['citTree']),
        RecursiveCachingIterator::TOSTRING_USE_KEY);
    $it->rewind(); $it->next();
    return $it->getChildren() === $it->getChildren();
});
class CitSubRec extends RecursiveCachingIterator {}
citShow('a subclass child is not the subclass', function () {
    $it = new CitSubRec(new RecursiveArrayIterator($GLOBALS['citTree']),
        RecursiveCachingIterator::TOSTRING_USE_KEY);
    $it->rewind(); $it->next();
    return get_class($it->getChildren());
});
citShow('flags are masked on the way down', function () {
    $it = new RecursiveCachingIterator(new RecursiveArrayIterator($GLOBALS['citTree']),
        1024 | RecursiveCachingIterator::TOSTRING_USE_KEY | RecursiveCachingIterator::FULL_CACHE);
    $it->rewind(); $it->next();
    return [$it->getFlags(), $it->getChildren()->getFlags()];
});
citShow('recursive walk', fn() => array_keys(iterator_to_array(new RecursiveIteratorIterator(
    new RecursiveCachingIterator(new RecursiveArrayIterator($GLOBALS['citTree']),
        RecursiveCachingIterator::TOSTRING_USE_KEY),
    RecursiveIteratorIterator::SELF_FIRST))));
/* The declared parameter is php's Iterator; the refusal is its body's. */
citShow('a plain Iterator is refused', fn() => new RecursiveCachingIterator(new ArrayIterator([1]),
    RecursiveCachingIterator::TOSTRING_USE_KEY));
citShow('the declared type', fn() => (string) (new ReflectionMethod('RecursiveCachingIterator',
    '__construct'))->getParameters()[0]->getType());
/* CATCH_GET_CHILD swallows a throw from any of the three steps. */
class CitThrowHas extends RecursiveArrayIterator {
    public function hasChildren(): bool { throw new RuntimeException('hc'); }
}
class CitThrowGet extends RecursiveArrayIterator {
    public function hasChildren(): bool { return true; }
    #[\ReturnTypeWillChange] public function getChildren() { return 42; }
}
foreach ([RecursiveCachingIterator::TOSTRING_USE_KEY,
          RecursiveCachingIterator::TOSTRING_USE_KEY | RecursiveCachingIterator::CATCH_GET_CHILD] as $citF) {
    citShow("throwing hasChildren, flags $citF", function () use ($citF) {
        $it = new RecursiveCachingIterator(new CitThrowHas(['a' => 1]), $citF);
        $it->rewind();
        return [$it->getFlags(), $it->hasChildren()];
    });
    citShow("throwing getChildren, flags $citF", function () use ($citF) {
        $it = new RecursiveCachingIterator(new CitThrowGet(['a' => 1]), $citF);
        $it->rewind();
        return [$it->getFlags(), $it->hasChildren()];
    });
}

/* Presentation: no properties anywhere, and no clone. */
citShow('present', fn() => [(array) new CachingIterator(new ArrayIterator([1])),
    serialize(new CachingIterator(new ArrayIterator([1]))),
    serialize(new RecursiveCachingIterator(new RecursiveArrayIterator([1])))]);
citShow('clone', fn() => clone new CachingIterator(new ArrayIterator([1])));
foreach (['rewind', 'valid', 'next', 'hasNext', '__toString', 'getFlags', 'getCache', 'count'] as $citM) {
    citShow("no ctor: $citM",
        fn() => (new ReflectionClass('CachingIterator'))->newInstanceWithoutConstructor()->$citM());
}
citShow('no ctor: hasChildren',
    fn() => (new ReflectionClass('RecursiveCachingIterator'))->newInstanceWithoutConstructor()->hasChildren());
restore_error_handler();
--EXPECT--
walk => array (  0 => 'x=1/true/1',  1 => 'y=2/true/2',  2 => 'z=3/false/3',)
hasNext is live => array (  0 => true,  1 => false,  2 => 'a',  3 => NULL,)
hasNext over an empty inner => false
past the end => array (  0 => false,  1 => 1,  2 => '',  3 => NULL,  4 => NULL,)
valid bit => array (  0 => 2,  1 => 65538,  2 => 2,)
ctor keeps 1024 => 1024
ctor masks 0x10000 => 1
setFlags keeps the high half => 273
two spellings at once => ValueError: CachingIterator::__construct(): Argument #2 ($flags) must contain only one of CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER
setFlags two at once => ValueError: CachingIterator::setFlags(): Argument #1 ($flags) must contain only one of CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER
unsetting CALL_TOSTRING => InvalidArgumentException: Unsetting flag CALL_TO_STRING is not possible
unsetting TOSTRING_USE_INNER => InvalidArgumentException: Unsetting flag TOSTRING_USE_INNER is not possible
a refused setFlags changes nothing => 1
use key => 'k'
use current => 'v'
use inner => 'INNER'
no spelling => BadMethodCallException: CachingIterator does not fetch string value (see CachingIterator::__construct)
the refusal names the receiver => BadMethodCallException: CitSub does not fetch string value (see CachingIterator::__construct)
cast is eager => Error: Object of class stdClass could not be converted to string
WARN: Array to string conversion
array element casts loudly => 'Array'
cache fills as it walks => array (  0 => '1:x',  1 => '2:x,y',  2 => '3:x,y,z',  3 => 'end:x,y,z',  4 => 'again:x',)
array access => array (  0 => 1,  1 => true,  2 => false,  3 => 9,  4 => 4,  5 => 3,)
WARN: Undefined array key "0"
WARN: Undefined array key "1"
WARN: Undefined array key "1.9"
a missing key is quoted whatever it was => array (  0 => NULL,  1 => NULL,  2 => NULL,)
an array key is refused => TypeError: CachingIterator::offsetGet(): Argument #1 ($key) must be of type string, array given
no cache: getCache => BadMethodCallException: CachingIterator does not use a full cache (see CachingIterator::__construct)
no cache: count => BadMethodCallException: CachingIterator does not use a full cache (see CachingIterator::__construct)
no cache: offsetGet => BadMethodCallException: CachingIterator does not use a full cache (see CachingIterator::__construct)
no cache: offsetExists => BadMethodCallException: CachingIterator does not use a full cache (see CachingIterator::__construct)
no cache: offsetSet => BadMethodCallException: CachingIterator does not use a full cache (see CachingIterator::__construct)
no cache: offsetUnset => BadMethodCallException: CachingIterator does not use a full cache (see CachingIterator::__construct)
children => array (  0 => 'a/false/null',  1 => 'b/true/RecursiveCachingIterator:2',  2 => 'd/false/null',  3 => 'end/false',)
the same child object twice => true
a subclass child is not the subclass => 'RecursiveCachingIterator'
flags are masked on the way down => array (  0 => 66818,  1 => 1282,)
recursive walk => array (  0 => 'a',  1 => 'b',  2 => 'c',  3 => 'd',)
a plain Iterator is refused => TypeError: RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, ArrayIterator given
the declared type => 'Iterator'
throwing hasChildren, flags 2 => RuntimeException: hc
throwing getChildren, flags 2 => TypeError: RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, int given
throwing hasChildren, flags 18 => array (  0 => 65554,  1 => false,)
throwing getChildren, flags 18 => array (  0 => 65554,  1 => false,)
present => array (  0 =>   array (  ),  1 => 'O:15:"CachingIterator":0:{}',  2 => 'O:24:"RecursiveCachingIterator":0:{}',)
clone => Error: Trying to clone an uncloneable object of class CachingIterator
no ctor: rewind => Error: The object is in an invalid state as the parent constructor was not called
no ctor: valid => Error: The object is in an invalid state as the parent constructor was not called
no ctor: next => Error: The object is in an invalid state as the parent constructor was not called
no ctor: hasNext => Error: The object is in an invalid state as the parent constructor was not called
no ctor: __toString => Error: The object is in an invalid state as the parent constructor was not called
no ctor: getFlags => Error: The object is in an invalid state as the parent constructor was not called
no ctor: getCache => Error: The object is in an invalid state as the parent constructor was not called
no ctor: count => Error: The object is in an invalid state as the parent constructor was not called
no ctor: hasChildren => Error: The object is in an invalid state as the parent constructor was not called
