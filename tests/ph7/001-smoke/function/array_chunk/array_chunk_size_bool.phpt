--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk()'s $length coerces a bool, as every int parameter does
--DESCRIPTION--
`array_chunk($a, true)` is a php program: an `int` parameter coerces a bool
like any other scalar, so the length is 1 — and `false` is 0, which is the
"must be greater than 0" ValueError rather than a type refusal. This engine
listed bool beside array, object and resource in a screen the builtin writes
for itself, so both spellings were a TypeError the shared signature screen
(which coerces, as php does) had already let through.
--FILE--
<?php
function acbShow($what, $fn) {
    try { $out = json_encode($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo "$what => $out\n";
}
/* A bool is a scalar an `int` parameter coerces: true is 1, false is 0 — and
 * 0 is what the ValueError is about, not the TYPE. */
acbShow('len true',     fn() => array_chunk([1, 2, 3], true));
acbShow('len false',    fn() => array_chunk([1, 2, 3], false));
acbShow('len 1',        fn() => array_chunk([1, 2, 3], 1));
acbShow("len '2'",      fn() => array_chunk([1, 2, 3], '2'));
acbShow('len 1.0',      fn() => array_chunk([1, 2, 3], 1.0));
acbShow("len 'abc'",    fn() => array_chunk([1, 2, 3], 'abc'));
acbShow('len []',       fn() => array_chunk([1, 2, 3], [1]));
acbShow('len object',   fn() => array_chunk([1, 2, 3], new stdClass));
/* The third parameter is declared `bool`, so every scalar reaches it. */
acbShow('keys true',    fn() => array_chunk([1, 2, 3], 2, true));
acbShow('keys false',   fn() => array_chunk([1, 2, 3], 2, false));
acbShow('keys 1',       fn() => array_chunk([1, 2, 3], 2, 1));
acbShow("keys 'x'",     fn() => array_chunk([1, 2, 3], 2, 'x'));
acbShow('keys []',      fn() => array_chunk([1, 2, 3], 2, [1]));
--EXPECT--
len true => [[1],[2],[3]]
len false => ValueError: array_chunk(): Argument #2 ($length) must be greater than 0
len 1 => [[1],[2],[3]]
len '2' => [[1,2],[3]]
len 1.0 => [[1],[2],[3]]
len 'abc' => TypeError: array_chunk(): Argument #2 ($length) must be of type int, string given
len [] => TypeError: array_chunk(): Argument #2 ($length) must be of type int, array given
len object => TypeError: array_chunk(): Argument #2 ($length) must be of type int, stdClass given
keys true => [[1,2],{"2":3}]
keys false => [[1,2],[3]]
keys 1 => [[1,2],{"2":3}]
keys 'x' => [[1,2],{"2":3}]
keys [] => TypeError: array_chunk(): Argument #3 ($preserve_keys) must be of type bool, array given
