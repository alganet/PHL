--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: array_rand() throws ValueError/TypeError on domain-invalid args
--FILE--
<?php
function ar_probe($fn): string {
    try {
        $fn();
        return "no-throw";
    } catch (\Throwable $e) {
        return get_class($e) . ": " . $e->getMessage();
    }
}
// empty array -> ValueError (fires even when $num is also out of range)
echo ar_probe(fn() => array_rand([])), "\n";
echo ar_probe(fn() => array_rand([], 5)), "\n";
// $num outside [1, count($array)] -> ValueError
echo ar_probe(fn() => array_rand([1, 2, 3], 0)), "\n";
echo ar_probe(fn() => array_rand([1, 2, 3], -1)), "\n";
echo ar_probe(fn() => array_rand([1, 2, 3], 4)), "\n";
// wrong types -> TypeError
echo ar_probe(fn() => array_rand("x")), "\n";
echo ar_probe(fn() => array_rand([1, 2, 3], [])), "\n";
echo ar_probe(fn() => array_rand([1, 2, 3], "abc")), "\n";
// a leading-numeric-junk / non-numeric-base string $num is a TypeError, not a
// silent coercion of its numeric prefix
echo ar_probe(fn() => array_rand([1, 2, 3], "2abc")), "\n";
echo ar_probe(fn() => array_rand([1, 2, 3], "0x1A")), "\n";
// a well-formed float-string coerces like a float value, then range-checks
echo ar_probe(fn() => array_rand([1, 2, 3], "1e3")), "\n";
// $num type-check fires before the empty-array body check (php ZPP ordering)
echo ar_probe(fn() => array_rand([], [])), "\n";
// valid calls still work: single key vs array of keys
$in = [10, 20, 30];
$k = array_rand($in);
echo (in_array($k, [0, 1, 2], true) ? "key-ok" : "key-bad"), "\n";
$keys = array_rand($in, 3);
echo (is_array($keys) && count($keys) === 3 ? "all-ok" : "all-bad"), "\n";
$one = array_rand($in, 1);
echo (is_int($one) ? "one-scalar-ok" : "one-bad"), "\n";
?>
--EXPECT--
ValueError: array_rand(): Argument #1 ($array) must not be empty
ValueError: array_rand(): Argument #1 ($array) must not be empty
ValueError: array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)
ValueError: array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)
ValueError: array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)
TypeError: array_rand(): Argument #1 ($array) must be of type array, string given
TypeError: array_rand(): Argument #2 ($num) must be of type int, array given
TypeError: array_rand(): Argument #2 ($num) must be of type int, string given
TypeError: array_rand(): Argument #2 ($num) must be of type int, string given
TypeError: array_rand(): Argument #2 ($num) must be of type int, string given
ValueError: array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)
TypeError: array_rand(): Argument #2 ($num) must be of type int, array given
key-ok
all-ok
one-scalar-ok
--CLEAN--
<?php
