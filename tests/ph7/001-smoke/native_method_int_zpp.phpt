--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native method's int parameter refuses a non-numeric string, as a builtin's does
--DESCRIPTION--
php's weak mode coerces a NUMERIC string to an int parameter and refuses every
other one — "x", "2abc" and "0x2" alike, because a numeric PREFIX is not enough.
Every builtin got that from PH7_IntArgResolve, called from its own body; a NATIVE
method has no body to call it from, so the check simply never ran for one. The
result was wrong ANSWERS rather than missing errors: seek('x') moved to offset 0,
setTimestamp('abc') set the epoch, DOMNodeList::item('zz') answered element 0 and
ReflectionClass::getMethods('nope') filtered everything out. The screen reads the
DECLARED type now, which covers both callee kinds from one place.
--FILE--
<?php
function nmiShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* The four families that were silently wrong, one per callee shape. */
nmiShow('seek non-numeric', fn() => (new ArrayIterator([1, 2, 3]))->seek('x'));
nmiShow('seek leading-numeric', fn() => (new ArrayIterator([1, 2, 3]))->seek('2abc'));
nmiShow('seek hex', fn() => (new ArrayIterator([1, 2, 3]))->seek('0x2'));
nmiShow('setTimestamp', fn() => (new DateTime('2020-01-01'))->setTimestamp('abc')->format('U'));
nmiShow('setDate', fn() => (new DateTime('2020-01-01'))->setDate('x', 5, 6)->format('Y-m-d'));
nmiShow('getMethods filter', fn() => count((new ReflectionClass('ArrayObject'))->getMethods('nope')));

/* A NUMERIC string still coerces, and so does an integral float — the screen
 * judges the declared type, it does not tighten weak mode. */
nmiShow('seek numeric string', function () {
    $it = new ArrayIterator([1, 2, 3]);
    $it->seek('2');
    return $it->current();
});
nmiShow('seek integral float', function () {
    $it = new ArrayIterator([1, 2, 3]);
    $it->seek(1.0);
    return $it->current();
});
nmiShow('setTimestamp numeric string', fn() => (new DateTime())->setTimestamp('86400')->format('Y-m-d'));

/* A BUILTIN's int parameter words the refusal the same way — the two callee
 * kinds share one screen now, and the builtin's own check agrees with it. */
nmiShow('builtin str_repeat', fn() => str_repeat('a', 'x'));
nmiShow('builtin substr', fn() => substr('abcdef', 'q'));
nmiShow('builtin intdiv', fn() => intdiv('7', 2));

/* An int arm the string CAN satisfy stays unscreened: a union carrying string
 * takes the string, and `mixed` takes anything. */
nmiShow('union with string', fn() => str_pad('x', 5, '-', STR_PAD_LEFT));
nmiShow('mixed parameter', fn() => in_array('x', ['x'], false));
--EXPECT--
seek non-numeric => TypeError: ArrayIterator::seek(): Argument #1 ($offset) must be of type int, string given
seek leading-numeric => TypeError: ArrayIterator::seek(): Argument #1 ($offset) must be of type int, string given
seek hex => TypeError: ArrayIterator::seek(): Argument #1 ($offset) must be of type int, string given
setTimestamp => TypeError: DateTime::setTimestamp(): Argument #1 ($timestamp) must be of type int, string given
setDate => TypeError: DateTime::setDate(): Argument #1 ($year) must be of type int, string given
getMethods filter => TypeError: ReflectionClass::getMethods(): Argument #1 ($filter) must be of type ?int, string given
seek numeric string => 3
seek integral float => 2
setTimestamp numeric string => '1970-01-02'
builtin str_repeat => TypeError: str_repeat(): Argument #2 ($times) must be of type int, string given
builtin substr => TypeError: substr(): Argument #2 ($offset) must be of type int, string given
builtin intdiv => 3
union with string => '----x'
mixed parameter => true
