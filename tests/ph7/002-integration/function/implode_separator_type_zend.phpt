--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's implode() specialization is DIRECT-call only; join() and the indirect forms differ (php half)
--SKIPIF--
skip: flaky
--DESCRIPTION--
implode() and join() are one function whose messages report the ARITY overload as already
resolved: with an $array in position #2, #1 is the SEPARATOR and must be a `string` -- never
the signature's `array|string`, which is only what the two arities accept between them -- and
an ARRAY in position #1 with a second argument is that same #1 error whatever #2 holds. PHL
reported the union instead (the central ZPP screen fired first, and one signature row cannot
narrow by arity), and it hardcoded `implode(): ` into every message so `join()` named the
wrong function.

php 8.5 reaches that contract only through a SPECIALIZED handler for a DIRECT,
compile-time-resolved `implode(...)` call. join(), `$f='implode'; $f(...)` and
call_user_func('implode', ...) fall back to a generic path that words three cases differently
(`array|string` for a bad separator, a #2 error when #1 is an array, and "ab" rather than a
throw for `implode(['a','b'], null)`). It is a call-FORM specialization, not a semantic rule --
it does not change with opcache off -- so PHL gives EVERY call form the direct-call contract,
which is the form real code writes and the one the corpus already pins. This half records php's
generic-path answers; PHL's uniform ones are in the non-_zend member.
--FILE--
<?php
class ImBare {}
class ImStr { public function __toString() { return "-"; } }

function t($label, $fn) {
    echo "$label: ";
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "-> the alias names ITSELF\n";
t('join(obj, arr)',  fn() => join(new ImBare(), ['a', 'b']));
t('join(arr, obj)',  fn() => join(['a'], new ImBare()));
t('join(arr, int)',  fn() => join(['a', 'b'], 1));
t('join(arr, null)', fn() => join(['a', 'b'], null));
t('join(str, int)',  fn() => join('-', 1));
t('join(str, null)', fn() => join('-', null));
t('join(str)',       fn() => join('-'));
t('join(arr, arr)',  fn() => join(['a', 'b'], ['c']));

echo "-> #1 is the SEPARATOR once #2 is there, so it must be a string\n";
t('implode(obj, arr)',  fn() => implode(new ImBare(), ['a', 'b']));
t('implode(res, arr)',  function () { $f = fopen('php://memory', 'r'); return implode($f, ['a', 'b']); });
t('implode(arr, obj)',  fn() => implode(['a'], new ImBare()));
t('implode(arr, int)',  fn() => implode(['a', 'b'], 1));
t('implode(arr, null)', fn() => implode(['a', 'b'], null));
t('implode(obj)',       fn() => implode(new ImBare()));

echo "-> what both engines accept, unchanged\n";
t('implode(arr)',        fn() => implode(['a', 'b']));
t('implode(str, arr)',   fn() => implode('-', ['a', 'b']));
t('join(str, arr)',      fn() => join('-', ['a', 'b']));
t('implode(int, arr)',   fn() => implode(1, ['a', 'b']));
t('implode(float, arr)', fn() => implode(1.5, ['a', 'b']));
t('implode(bool, arr)',  fn() => implode(true, ['a', 'b']));
t('implode(Str, arr)',   fn() => implode(new ImStr(), ['a', 'b']));
t('implode(str, [])',    fn() => implode('-', []));
t('implode(str, assoc)', fn() => implode('-', ['x' => 'a', 'y' => 'b']));
?>
--EXPECT--
-> the alias names ITSELF
join(obj, arr): TypeError: join(): Argument #1 ($separator) must be of type array|string, ImBare given
join(arr, obj): TypeError: join(): Argument #2 ($array) must be of type ?array, ImBare given
join(arr, int): TypeError: join(): Argument #2 ($array) must be of type ?array, int given
join(arr, null): string(2) "ab"
join(str, int): TypeError: join(): Argument #2 ($array) must be of type ?array, int given
join(str, null): TypeError: join(): If argument #1 ($separator) is of type string, argument #2 ($array) must be of type array, null given
join(str): TypeError: join(): If argument #1 ($separator) is of type string, argument #2 ($array) must be of type array, null given
join(arr, arr): TypeError: join(): Argument #1 ($separator) must be of type string, array given
-> #1 is the SEPARATOR once #2 is there, so it must be a string
implode(obj, arr): TypeError: implode(): Argument #1 ($separator) must be of type string, ImBare given
implode(res, arr): TypeError: implode(): Argument #1 ($separator) must be of type string, resource given
implode(arr, obj): TypeError: implode(): Argument #1 ($separator) must be of type string, array given
implode(arr, int): TypeError: implode(): Argument #1 ($separator) must be of type string, array given
implode(arr, null): TypeError: implode(): Argument #1 ($separator) must be of type string, array given
implode(obj): TypeError: implode(): If argument #1 ($separator) is of type string, argument #2 ($array) must be of type array, null given
-> what both engines accept, unchanged
implode(arr): string(2) "ab"
implode(str, arr): string(3) "a-b"
join(str, arr): string(3) "a-b"
implode(int, arr): string(3) "a1b"
implode(float, arr): string(5) "a1.5b"
implode(bool, arr): string(3) "a1b"
implode(Str, arr): string(3) "a-b"
implode(str, []): string(0) ""
implode(str, assoc): string(3) "a-b"
