--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RegexIterator is a C class and its transforming modes write the cache
--DESCRIPTION--
php's accept() reads the decorator's CACHED current()/key() and, in every mode
but MATCH, writes the transformed value BACK into that slot — which is why the
class declares no current() of its own. Two answers follow from that and were
both wrong while the class was embedded PHP: a REPLACE under USE_KEY replaces
into the KEY (current() keeps the original), and an ARRAY current() is refused
outright instead of being matched as the string "Array". The declaration is
php's too: an ?string $replacement, the five-mode ValueError, and a pattern that
does not compile is an InvalidArgumentException from `new` rather than a warning
per element.
--FILE--
<?php
function srinShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
class SrinLoud extends RegexIterator {
    public function accept(): bool { return parent::accept(); }
}

/* MATCH filters and leaves the value alone; the keys are the inner's. */
srinShow('match', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['apple', 'banana', 'avocado']), '/^a/')));
srinShow('use key', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a1' => 'x', 'b2' => 'y']), '/^a/',
        RegexIterator::MATCH, RegexIterator::USE_KEY)));
srinShow('invert', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['apple', 'banana']), '/^a/',
        RegexIterator::MATCH, RegexIterator::INVERT_MATCH)));

/* The transforming modes: current() is the inherited cached one. */
srinShow('get match', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a1', 'b2', 'a3']), '/^a(\d)/', RegexIterator::GET_MATCH)));
srinShow('all matches', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a1a2', 'b']), '/a(\d)/', RegexIterator::ALL_MATCHES)));
srinShow('split', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a,b', 'c']), '/,/', RegexIterator::SPLIT)));
srinShow('replace', function () {
    $i = new RegexIterator(new ArrayIterator(['a1', 'b2']), '/\d/', RegexIterator::REPLACE);
    $i->replacement = 'X';
    return iterator_to_array($i);
});
/* php replaces into the KEY here, and current() keeps the original value. */
srinShow('replace keyed', function () {
    $i = new RegexIterator(new ArrayIterator(['k1' => 'v1', 'k2' => 'v2']), '/\d/',
        RegexIterator::REPLACE, RegexIterator::USE_KEY);
    $i->replacement = 'Z';
    $out = [];
    foreach ($i as $k => $v) { $out[] = $k . '=' . $v; }
    return $out;
});
/* A null $replacement is the empty string, not a refusal. */
srinShow('replace null', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a1']), '/\d/', RegexIterator::REPLACE)));
/* An INVERTed transforming mode still yields the transformed cache. */
srinShow('invert get match', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a1', 'bb']), '/^a(\d)/',
        RegexIterator::GET_MATCH, RegexIterator::INVERT_MATCH)));
srinShow('preg flags', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator(['a1']), '/(\d)/',
        RegexIterator::GET_MATCH, 0, PREG_OFFSET_CAPTURE)));

/* An ARRAY current() is refused before any matching happens. */
srinShow('array current', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator([[1, 2], 'a']), '/a/')));
srinShow('array current get match', fn() => iterator_to_array(
    new RegexIterator(new ArrayIterator([[1, 2], 'a']), '/a/', RegexIterator::GET_MATCH)));

/* The cache is the class, exactly as for the decorators it extends. */
srinShow('before rewind', function () {
    $i = new RegexIterator(new ArrayIterator(['a']), '/a/');
    return [$i->valid(), $i->accept(), $i->current(), $i->key()];
});
srinShow('cached current', function () {
    $a = new ArrayIterator(['a', 'ab', 'ac']);
    $i = new RegexIterator($a, '/a/');
    $i->rewind();
    $a->next(); $a->next();
    return [$i->current(), $i->key(), $i->valid()];
});
srinShow('subclass accept', fn() => iterator_to_array(
    new SrinLoud(new ArrayIterator(['aa', 'b']), '/a/')));

/* Accessors, and the one setter php screens. */
srinShow('accessors', function () {
    $i = new RegexIterator(new ArrayIterator([]), '/x/');
    $before = [$i->getRegex(), $i->getMode(), $i->getFlags(), $i->getPregFlags()];
    $i->setMode(RegexIterator::SPLIT);
    $i->setFlags(RegexIterator::USE_KEY);
    $i->setPregFlags(2);
    return array_merge($before, [$i->getMode(), $i->getFlags(), $i->getPregFlags()]);
});
srinShow('set mode range', function () {
    (new RegexIterator(new ArrayIterator([]), '/x/'))->setMode(99);
});
srinShow('ctor mode range', fn() => new RegexIterator(new ArrayIterator([]), '/x/', 7));
srinShow('ctor bad pattern', fn() => new RegexIterator(new ArrayIterator([]), 'nodelim'));
srinShow('ctor aggregate', fn() => new RegexIterator(new ArrayObject([1]), '/x/'));

/* The declaration: one ?string property, and no cloning. */
srinShow('replacement coerces', function () {
    $i = new RegexIterator(new ArrayIterator([]), '/x/');
    $was = $i->replacement;
    $i->replacement = 5;
    return [$was, $i->replacement];
});
srinShow('replacement type', function () {
    $i = new RegexIterator(new ArrayIterator([]), '/x/');
    $i->replacement = [];
});
srinShow('properties', function () {
    $out = [];
    foreach ((new ReflectionClass('RegexIterator'))->getProperties() as $p) {
        $out[] = ($p->hasType() ? (string) $p->getType() . ' ' : '') . '$' . $p->getName();
    }
    return [$out, array_keys(get_object_vars(new RegexIterator(new ArrayIterator([]), '/x/')))];
});
srinShow('uncloneable', fn() => clone new RegexIterator(new ArrayIterator([]), '/x/'));
srinShow('constants', fn() => [RegexIterator::USE_KEY, RegexIterator::INVERT_MATCH,
    RegexIterator::MATCH, RegexIterator::GET_MATCH, RegexIterator::ALL_MATCHES,
    RegexIterator::SPLIT, RegexIterator::REPLACE]);
srinShow('hierarchy', fn() => [get_parent_class('RegexIterator'),
    (new ReflectionClass('RegexIterator'))->isInternal()]);
--EXPECT--
match => array (  0 => 'apple',  2 => 'avocado',)
use key => array (  'a1' => 'x',)
invert => array (  1 => 'banana',)
get match => array (  0 =>   array (    0 => 'a1',    1 => '1',  ),  2 =>   array (    0 => 'a3',    1 => '3',  ),)
all matches => array (  0 =>   array (    0 =>     array (      0 => 'a1',      1 => 'a2',    ),    1 =>     array (      0 => '1',      1 => '2',    ),  ),)
split => array (  0 =>   array (    0 => 'a',    1 => 'b',  ),)
replace => array (  0 => 'aX',  1 => 'bX',)
replace keyed => array (  0 => 'kZ=v1',  1 => 'kZ=v2',)
replace null => array (  0 => 'a',)
invert get match => array (  1 =>   array (  ),)
preg flags => array (  0 =>   array (    0 =>     array (      0 => '1',      1 => 1,    ),    1 =>     array (      0 => '1',      1 => 1,    ),  ),)
array current => array (  1 => 'a',)
array current get match => array (  1 =>   array (    0 => 'a',  ),)
before rewind => array (  0 => false,  1 => false,  2 => NULL,  3 => NULL,)
cached current => array (  0 => 'a',  1 => 0,  2 => true,)
subclass accept => array (  0 => 'aa',)
accessors => array (  0 => '/x/',  1 => 0,  2 => 0,  3 => 0,  4 => 3,  5 => 1,  6 => 2,)
set mode range => ValueError: RegexIterator::setMode(): Argument #1 ($mode) must be RegexIterator::MATCH, RegexIterator::GET_MATCH, RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE
ctor mode range => ValueError: RegexIterator::__construct(): Argument #3 ($mode) must be RegexIterator::MATCH, RegexIterator::GET_MATCH, RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE
ctor bad pattern => InvalidArgumentException: RegexIterator::__construct(): Delimiter must not be alphanumeric, backslash, or NUL byte
ctor aggregate => TypeError: RegexIterator::__construct(): Argument #1 ($iterator) must be of type Iterator, ArrayObject given
replacement coerces => array (  0 => NULL,  1 => '5',)
replacement type => TypeError: Cannot assign array to property RegexIterator::$replacement of type ?string
properties => array (  0 =>   array (    0 => '?string $replacement',  ),  1 =>   array (    0 => 'replacement',  ),)
uncloneable => Error: Trying to clone an uncloneable object of class RegexIterator
constants => array (  0 => 1,  1 => 2,  2 => 0,  3 => 1,  4 => 2,  5 => 3,  6 => 4,)
hierarchy => array (  0 => 'FilterIterator',  1 => true,)
