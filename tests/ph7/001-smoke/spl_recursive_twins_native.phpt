--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The three recursive filter twins: ParentIterator and the callback/regex pair
--DESCRIPTION--
php builds each of these on a filter it already has, and each one adds exactly one
rule the plain class does not have. ParentIterator's accept() IS the question "has
the current element children?", asked of the INNER iterator — so overriding
hasChildren() on the ParentIterator itself changes nothing and overriding it on the
inner RecursiveIterator changes everything. RecursiveCallbackFilterIterator has to
carry its callback into the child. RecursiveRegexIterator accepts a non-empty ARRAY
current() ahead of the regex, whatever the mode, USE_KEY or INVERT_MATCH say —
which is what lets a walk descend into a container and match only the leaves — and
its getChildren() carries the four regex arguments down but never $replacement.
Every one of the three builds its child from the CALLED class, and every
diagnostic names the class that was constructed.
--FILE--
<?php
function rtwShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
$rtwTree = ['x' => 1, 'y' => ['a' => 2, 'b' => ['c' => 3]], 'z' => 4];

/* ParentIterator keeps only the elements that HAVE children. */
rtwShow('parent walk', fn() => array_keys(iterator_to_array(
    new ParentIterator(new RecursiveArrayIterator($GLOBALS['rtwTree'])))));
rtwShow('parent recursive', function () {
    $it = new RecursiveIteratorIterator(
        new ParentIterator(new RecursiveArrayIterator($GLOBALS['rtwTree'])),
        RecursiveIteratorIterator::SELF_FIRST);
    $out = [];
    foreach ($it as $k => $v) { $out[] = $it->getDepth() . ':' . $k; }
    return $out;
});

/* accept() asks the INNER iterator, so this override is never consulted... */
class RtwDeafParent extends ParentIterator {
    public function hasChildren(): bool { echo "[RtwDeafParent::hasChildren]"; return false; }
}
rtwShow('own hasChildren ignored', function () {
    $it = new RtwDeafParent(new RecursiveArrayIterator($GLOBALS['rtwTree']));
    $it->rewind();
    return [$it->key(), $it->accept()];
});
/* ...while the inner iterator's own IS. */
class RtwLoudInner extends RecursiveArrayIterator {
    public function hasChildren(): bool { echo '[inner]'; return parent::hasChildren(); }
}
rtwShow('inner hasChildren asked', function () {
    $it = new ParentIterator(new RtwLoudInner($GLOBALS['rtwTree']));
    $it->rewind();
    return $it->accept();
});
/* Nothing fetched yet: php answers the first element's, not a refusal. */
rtwShow('accept before rewind', fn() =>
    (new ParentIterator(new RecursiveArrayIterator($GLOBALS['rtwTree'])))->accept());
/* getChildren() builds the CALLED class. */
class RtwSubParent extends ParentIterator {}
rtwShow('parent child class', function () {
    $it = new RtwSubParent(new RecursiveArrayIterator($GLOBALS['rtwTree']));
    $it->rewind();
    return get_class($it->getChildren());
});

/* RecursiveCallbackFilterIterator: the callback sees the cached pair and the inner
 * iterator, and the CHILD is filtered by the same callback. */
rtwShow('callback walk', function () {
    $seen = [];
    $it = new RecursiveCallbackFilterIterator(
        new RecursiveArrayIterator($GLOBALS['rtwTree']),
        function ($cur, $key, $inner) use (&$seen) {
            $seen[] = $key . '@' . get_class($inner);
            return true;
        });
    foreach (new RecursiveIteratorIterator($it, RecursiveIteratorIterator::SELF_FIRST) as $v) {}
    return $seen;
});
class RtwSubCallback extends RecursiveCallbackFilterIterator {}
rtwShow('callback child carries it', function () {
    $n = 0;
    $it = new RtwSubCallback(new RecursiveArrayIterator($GLOBALS['rtwTree']),
        function ($c) use (&$n) { $n++; return true; });
    $it->rewind();
    $it->next();
    $child = $it->getChildren();
    $before = $n;
    $keys = array_keys(iterator_to_array($child));
    return [get_class($child), $keys, $n - $before];
});

/* RecursiveRegexIterator: an ARRAY current() is accepted when non-empty, ahead of
 * the regex and of every flag; a leaf is matched as RegexIterator matches it. */
rtwShow('regex keeps containers', fn() => array_keys(iterator_to_array(
    new RecursiveRegexIterator(
        new RecursiveArrayIterator(['e' => [], 'f' => ['g' => 1], 'h' => 'ab', 'i' => 'zz']),
        '/a/'))));
rtwShow('regex container beats USE_KEY', fn() => array_keys(iterator_to_array(
    new RecursiveRegexIterator(new RecursiveArrayIterator(['yy' => ['a' => 1], 'zz' => ['c' => 1]]),
        '/y/', RegexIterator::MATCH, RegexIterator::USE_KEY))));
rtwShow('regex container beats INVERT', fn() => array_keys(iterator_to_array(
    new RecursiveRegexIterator(new RecursiveArrayIterator(['yy' => ['a' => 1]]),
        '/y/', RegexIterator::MATCH, RegexIterator::INVERT_MATCH))));
rtwShow('regex container is not rewritten', function () {
    $it = new RecursiveRegexIterator(new RecursiveArrayIterator(['yy' => ['a' => 1]]),
        '/y/', RegexIterator::REPLACE, RegexIterator::USE_KEY);
    $it->replacement = 'Z';
    $it->rewind();
    return [$it->key(), $it->current()];
});
rtwShow('regex leaves are matched', function () {
    $it = new RecursiveIteratorIterator(
        new RecursiveRegexIterator(new RecursiveArrayIterator(['g' => ['a1', 'b2'], 'h' => 'a3']),
            '/^a/'));
    return array_values(iterator_to_array($it, false));
});
class RtwSubRegex extends RecursiveRegexIterator {}
rtwShow('regex child carries the four', function () {
    $it = new RtwSubRegex(new RecursiveArrayIterator(['y' => ['a' => 'bb']]), '/b/',
        RegexIterator::GET_MATCH, RegexIterator::USE_KEY, PREG_OFFSET_CAPTURE);
    $it->replacement = 'REP';
    $it->rewind();
    $child = $it->getChildren();
    return [get_class($child), $child->getRegex(), $child->getMode(), $child->getFlags(),
        $child->getPregFlags(), $child->replacement];
});

/* Every diagnostic names the class that was constructed. */
rtwShow('parent bad inner', fn() => new ParentIterator(new ArrayIterator([1])));
rtwShow('callback bad inner', fn() => new RecursiveCallbackFilterIterator(new ArrayIterator([1]),
    fn() => true));
rtwShow('callback bad callable', fn() => new RecursiveCallbackFilterIterator(
    new RecursiveArrayIterator([1]), 'rtw_no_such_function'));
rtwShow('regex bad inner', fn() => new RecursiveRegexIterator(new ArrayIterator([1]), '/a/'));
rtwShow('regex bad pattern', fn() => new RecursiveRegexIterator(new RecursiveArrayIterator([1]),
    'nodelim'));
rtwShow('regex bad mode', fn() => new RecursiveRegexIterator(new RecursiveArrayIterator([1]),
    '/a/', 99));
/* setMode is RegexIterator's own method and keeps naming it. */
rtwShow('inherited setMode', function () {
    $it = new RecursiveRegexIterator(new RecursiveArrayIterator([1]), '/a/');
    $it->setMode(99);
});

/* A subclass that never called the parent constructor refuses every method. */
foreach (['ParentIterator' => ['accept'],
          'RecursiveCallbackFilterIterator' => ['accept', 'hasChildren', 'getChildren'],
          'RecursiveRegexIterator' => ['accept', 'hasChildren', 'getChildren']] as $cls => $ms) {
    foreach ($ms as $m) {
        rtwShow("no ctor $cls::$m",
            fn() => (new ReflectionClass($cls))->newInstanceWithoutConstructor()->$m());
    }
}

/* php refuses to clone any dual iterator, and each serializes as no properties. */
foreach ([new ParentIterator(new RecursiveArrayIterator([1])),
          new RecursiveCallbackFilterIterator(new RecursiveArrayIterator([1]), fn() => true),
          new RecursiveRegexIterator(new RecursiveArrayIterator([1]), '/a/')] as $rtwObj) {
    rtwShow('clone ' . get_class($rtwObj), fn() => clone $rtwObj);
}
rtwShow('serialize', fn() => serialize(new ParentIterator(new RecursiveArrayIterator([1]))));
rtwShow('regex props', fn() => (array) new RecursiveRegexIterator(
    new RecursiveArrayIterator([1]), '/a/'));
--EXPECT--
parent walk => array (  0 => 'y',)
parent recursive => array (  0 => '0:y',  1 => '1:b',)
own hasChildren ignored => array (  0 => 'y',  1 => true,)
[inner][inner][inner]inner hasChildren asked => true
accept before rewind => false
parent child class => 'RtwSubParent'
callback walk => array (  0 => 'x@RecursiveArrayIterator',  1 => 'y@RecursiveArrayIterator',  2 => 'a@RecursiveArrayIterator',  3 => 'b@RecursiveArrayIterator',  4 => 'c@RecursiveArrayIterator',  5 => 'z@RecursiveArrayIterator',)
callback child carries it => array (  0 => 'RtwSubCallback',  1 =>   array (    0 => 'a',    1 => 'b',  ),  2 => 2,)
regex keeps containers => array (  0 => 'f',  1 => 'h',)
regex container beats USE_KEY => array (  0 => 'yy',  1 => 'zz',)
regex container beats INVERT => array (  0 => 'yy',)
regex container is not rewritten => array (  0 => 'yy',  1 =>   array (    'a' => 1,  ),)
regex leaves are matched => array (  0 => 'a1',  1 => 'a3',)
regex child carries the four => array (  0 => 'RtwSubRegex',  1 => '/b/',  2 => 1,  3 => 1,  4 => 256,  5 => NULL,)
parent bad inner => TypeError: ParentIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, ArrayIterator given
callback bad inner => TypeError: RecursiveCallbackFilterIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, ArrayIterator given
callback bad callable => TypeError: RecursiveCallbackFilterIterator::__construct(): Argument #2 ($callback) must be a valid callback, function "rtw_no_such_function" not found or invalid function name
regex bad inner => TypeError: RecursiveRegexIterator::__construct(): Argument #1 ($iterator) must be of type RecursiveIterator, ArrayIterator given
regex bad pattern => InvalidArgumentException: RecursiveRegexIterator::__construct(): Delimiter must not be alphanumeric, backslash, or NUL byte
regex bad mode => ValueError: RecursiveRegexIterator::__construct(): Argument #3 ($mode) must be RegexIterator::MATCH, RegexIterator::GET_MATCH, RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE
inherited setMode => ValueError: RegexIterator::setMode(): Argument #1 ($mode) must be RegexIterator::MATCH, RegexIterator::GET_MATCH, RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE
no ctor ParentIterator::accept => Error: The object is in an invalid state as the parent constructor was not called
no ctor RecursiveCallbackFilterIterator::accept => Error: The object is in an invalid state as the parent constructor was not called
no ctor RecursiveCallbackFilterIterator::hasChildren => Error: The object is in an invalid state as the parent constructor was not called
no ctor RecursiveCallbackFilterIterator::getChildren => Error: The object is in an invalid state as the parent constructor was not called
no ctor RecursiveRegexIterator::accept => Error: The object is in an invalid state as the parent constructor was not called
no ctor RecursiveRegexIterator::hasChildren => Error: The object is in an invalid state as the parent constructor was not called
no ctor RecursiveRegexIterator::getChildren => Error: The object is in an invalid state as the parent constructor was not called
clone ParentIterator => Error: Trying to clone an uncloneable object of class ParentIterator
clone RecursiveCallbackFilterIterator => Error: Trying to clone an uncloneable object of class RecursiveCallbackFilterIterator
clone RecursiveRegexIterator => Error: Trying to clone an uncloneable object of class RecursiveRegexIterator
serialize => 'O:14:"ParentIterator":0:{}'
regex props => array (  'replacement' => NULL,)
