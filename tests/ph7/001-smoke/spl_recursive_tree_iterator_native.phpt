--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RecursiveTreeIterator draws the walk it inherits
--DESCRIPTION--
It is RecursiveIteratorIterator with a string built around each element, and the
drawing is why php wraps whatever it is handed in a RecursiveCachingIterator:
the ASCII branches need to know whether a level has a NEXT element, and hasNext()
is the one question only that decorator answers. So every sub-iterator here is a
RecursiveCachingIterator, and `$cachingIteratorFlags` is what the wrapper is
built with — CATCH_GET_CHILD when the caller says nothing, and EXACTLY what the
caller says otherwise, the catch included. The prefix is six parts: a fixed LEFT,
one MID per level above this one, one END for this level (both chosen by whether
that level has a next element) and a fixed RIGHT; current() is prefix + entry +
postfix and key() is prefix + key + postfix, each bypassable through a flag that
lives in the same word as the inherited CATCH_GET_CHILD. getEntry() renders an
ARRAY as the word "Array" without saying anything about it.
--FILE--
<?php
function rtiShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
$rtiTree = ['a' => 1, 'b' => ['c' => 2, 'd' => ['e' => 3]], 'f' => 4];

rtiShow('default render', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator($GLOBALS['rtiTree']));
    $out = [];
    foreach ($it as $k => $v) { $out[] = json_encode($k) . ' ' . json_encode($v); }
    return $out;
});
rtiShow('the three parts', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator($GLOBALS['rtiTree']));
    $out = [];
    foreach ($it as $v) {
        $out[] = json_encode([$it->getPrefix(), $it->getEntry(), $it->getPostfix()]);
    }
    return $out;
});
rtiShow('every part replaced', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator($GLOBALS['rtiTree']));
    $it->setPrefixPart(RecursiveTreeIterator::PREFIX_LEFT, '<');
    $it->setPrefixPart(RecursiveTreeIterator::PREFIX_MID_HAS_NEXT, '| ');
    $it->setPrefixPart(RecursiveTreeIterator::PREFIX_MID_LAST, '  ');
    $it->setPrefixPart(RecursiveTreeIterator::PREFIX_END_HAS_NEXT, '+-');
    $it->setPrefixPart(RecursiveTreeIterator::PREFIX_END_LAST, '\\-');
    $it->setPrefixPart(RecursiveTreeIterator::PREFIX_RIGHT, '>');
    $it->setPostfix('!');
    $out = [];
    foreach ($it as $v) { $out[] = $v; }
    return $out;
});
rtiShow('setPrefixPart bounds', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator([1]));
    $out = [];
    foreach ([0, 5, 6, -1] as $p) {
        try { $it->setPrefixPart($p, 'x'); $out[] = "$p ok"; }
        catch (Throwable $e) { $out[] = "$p " . get_class($e) . ': ' . $e->getMessage(); }
    }
    return $out;
});
/* The four flag spellings over one tree. */
foreach ([0,
          RecursiveTreeIterator::BYPASS_KEY,
          RecursiveTreeIterator::BYPASS_CURRENT,
          RecursiveTreeIterator::BYPASS_CURRENT | RecursiveTreeIterator::BYPASS_KEY] as $rtiF) {
    rtiShow("flags $rtiF", function () use ($rtiF) {
        $it = new RecursiveTreeIterator(new RecursiveArrayIterator($GLOBALS['rtiTree']), $rtiF);
        $out = [];
        foreach ($it as $k => $v) { $out[] = json_encode($k) . ' ' . json_encode($v); }
        return $out;
    });
}
/* The wrapper, and what $cachingIteratorFlags does to it. */
rtiShow('the sub-iterator is a caching one', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator(['a' => 1]));
    $it->rewind();
    return [get_class($it->getSubIterator()), get_class($it->getInnerIterator()),
        $it->getSubIterator()->getFlags()];
});
rtiShow('the caller replaces the default flags', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator(['a' => 1]),
        RecursiveTreeIterator::BYPASS_KEY, CachingIterator::TOSTRING_USE_KEY);
    $it->rewind();
    return [$it->getSubIterator()->getFlags(), $it->current()];
});
/* getEntry()'s own conversion. */
rtiShow('an array entry', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator(['a' => ['b' => 1]]));
    $it->rewind();
    return [$it->getEntry(), $it->current(), $it->key()];
});
rtiShow('an int entry', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator([42]));
    $it->rewind();
    return $it->getEntry();
});
class RtiStringy { public function __toString(): string { return 'STR'; } }
rtiShow('a Stringable entry', function () {
    /* CHILD_ARRAYS_ONLY: an object element otherwise HAS children here, and
     * building them is `new RecursiveArrayIterator($obj)` — a php deprecation
     * that has nothing to do with the tree. */
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator([new RtiStringy],
        RecursiveArrayIterator::CHILD_ARRAYS_ONLY));
    $it->rewind();
    return $it->getEntry();
});
rtiShow('an unstringable entry', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator([new stdClass],
        RecursiveArrayIterator::CHILD_ARRAYS_ONLY));
    $it->rewind();
    return $it->getEntry();
});
rtiShow('past the end', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator([1]));
    $it->rewind(); $it->next();
    return [$it->valid(), $it->getEntry(), $it->getPrefix(), $it->getPostfix()];
});
/* The traversal is still RecursiveIteratorIterator's. */
rtiShow('mode and depth', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator(['a' => ['b' => 1]]),
        RecursiveTreeIterator::BYPASS_KEY, RecursiveTreeIterator::CATCH_GET_CHILD,
        RecursiveTreeIterator::CHILD_FIRST);
    $out = [];
    foreach ($it as $v) { $out[] = $it->getDepth() . ':' . $v; }
    return $out;
});
rtiShow('setMaxDepth', function () {
    $it = new RecursiveTreeIterator(new RecursiveArrayIterator(['a' => ['b' => 1]]));
    $it->setMaxDepth(0);
    $out = [];
    foreach ($it as $v) { $out[] = $v; }
    return [$out, $it->getMaxDepth()];
});
class RtiAgg implements IteratorAggregate {
    public function getIterator(): Iterator { return new RecursiveArrayIterator(['q' => ['r' => 1]]); }
}
rtiShow('an IteratorAggregate source', function () {
    $it = new RecursiveTreeIterator(new RtiAgg);
    $out = [];
    foreach ($it as $v) { $out[] = $v; }
    return $out;
});
/* The refusals: php's ZPP takes a bare object here and the wrapper decides. */
rtiShow('a non-object', fn() => new RecursiveTreeIterator(5));
rtiShow('an array', fn() => new RecursiveTreeIterator([1]));
rtiShow('a plain iterator', fn() => new RecursiveTreeIterator(new ArrayIterator([1])));
rtiShow('a plain object', fn() => new RecursiveTreeIterator(new stdClass));
rtiShow('a string $flags', fn() => new RecursiveTreeIterator(new RecursiveArrayIterator([1]), 'x'));
rtiShow('the declared type', fn() => (string) (new ReflectionMethod('RecursiveTreeIterator',
    '__construct'))->getParameters()[0]->getType());
/* Presentation, and the uninitialized refusal that names this class. */
rtiShow('present', fn() => [(array) new RecursiveTreeIterator(new RecursiveArrayIterator([1])),
    serialize(new RecursiveTreeIterator(new RecursiveArrayIterator([1])))]);
rtiShow('clone', fn() => clone new RecursiveTreeIterator(new RecursiveArrayIterator([1])));
foreach (['getPrefix', 'getEntry', 'getPostfix', 'key', 'current'] as $rtiM) {
    rtiShow("no ctor: $rtiM",
        fn() => (new ReflectionClass('RecursiveTreeIterator'))->newInstanceWithoutConstructor()->$rtiM());
}
--EXPECT--
default render => array (  0 => '"a" "|-1"',  1 => '"b" "|-Array"',  2 => '"c" "| |-2"',  3 => '"d" "| \\\\-Array"',  4 => '"e" "|   \\\\-3"',  5 => '"f" "\\\\-4"',)
the three parts => array (  0 => '["|-","1",""]',  1 => '["|-","Array",""]',  2 => '["| |-","2",""]',  3 => '["| \\\\-","Array",""]',  4 => '["|   \\\\-","3",""]',  5 => '["\\\\-","4",""]',)
every part replaced => array (  0 => '<+->1!',  1 => '<+->Array!',  2 => '<| +->2!',  3 => '<| \\->Array!',  4 => '<|   \\->3!',  5 => '<\\->4!',)
setPrefixPart bounds => array (  0 => '0 ok',  1 => '5 ok',  2 => '6 ValueError: RecursiveTreeIterator::setPrefixPart(): Argument #1 ($part) must be a RecursiveTreeIterator::PREFIX_* constant',  3 => '-1 ValueError: RecursiveTreeIterator::setPrefixPart(): Argument #1 ($part) must be a RecursiveTreeIterator::PREFIX_* constant',)
flags 0 => array (  0 => '"|-a" "|-1"',  1 => '"|-b" "|-Array"',  2 => '"| |-c" "| |-2"',  3 => '"| \\\\-d" "| \\\\-Array"',  4 => '"|   \\\\-e" "|   \\\\-3"',  5 => '"\\\\-f" "\\\\-4"',)
flags 8 => array (  0 => '"a" "|-1"',  1 => '"b" "|-Array"',  2 => '"c" "| |-2"',  3 => '"d" "| \\\\-Array"',  4 => '"e" "|   \\\\-3"',  5 => '"f" "\\\\-4"',)
flags 4 => array (  0 => '"|-a" 1',  1 => '"|-b" {"c":2,"d":{"e":3}}',  2 => '"| |-c" 2',  3 => '"| \\\\-d" {"e":3}',  4 => '"|   \\\\-e" 3',  5 => '"\\\\-f" 4',)
flags 12 => array (  0 => '"a" 1',  1 => '"b" {"c":2,"d":{"e":3}}',  2 => '"c" 2',  3 => '"d" {"e":3}',  4 => '"e" 3',  5 => '"f" 4',)
the sub-iterator is a caching one => array (  0 => 'RecursiveCachingIterator',  1 => 'RecursiveCachingIterator',  2 => 65552,)
the caller replaces the default flags => array (  0 => 65538,  1 => '\\-1',)
an array entry => array (  0 => 'Array',  1 => '\\-Array',  2 => 'a',)
an int entry => '42'
a Stringable entry => 'STR'
an unstringable entry => Error: Object of class stdClass could not be converted to string
past the end => array (  0 => false,  1 => '',  2 => '\\-',  3 => '',)
mode and depth => array (  0 => '1:  \\-1',  1 => '0:\\-Array',)
setMaxDepth => array (  0 =>   array (    0 => '\\-Array',  ),  1 => 0,)
an IteratorAggregate source => array (  0 => '\\-Array',  1 => '  \\-1',)
a non-object => TypeError: RecursiveTreeIterator::__construct(): Argument #1 ($iterator) must be of type object, int given
an array => TypeError: RecursiveTreeIterator::__construct(): Argument #1 ($iterator) must be of type object, array given
a plain iterator => TypeError: RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, ArrayIterator given
a plain object => TypeError: RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, stdClass given
a string $flags => TypeError: RecursiveTreeIterator::__construct(): Argument #2 ($flags) must be of type int, string given
the declared type => 'RecursiveIterator|IteratorAggregate'
present => array (  0 =>   array (  ),  1 => 'O:21:"RecursiveTreeIterator":0:{}',)
clone => Error: Trying to clone an uncloneable object of class RecursiveTreeIterator
no ctor: getPrefix => Error: The RecursiveTreeIterator instance wasn't initialized properly
no ctor: getEntry => Error: The RecursiveTreeIterator instance wasn't initialized properly
no ctor: getPostfix => Error: The RecursiveTreeIterator instance wasn't initialized properly
no ctor: key => Error: The RecursiveTreeIterator instance wasn't initialized properly
no ctor: current => Error: The RecursiveTreeIterator instance wasn't initialized properly
