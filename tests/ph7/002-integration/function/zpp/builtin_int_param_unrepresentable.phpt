--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An `int` builtin parameter refuses a value no int can hold, whichever builtin it is
--FILE--
<?php
// php's ZPP refuses a float (or float-string) outside the signed 64-bit range,
// NaN, an infinity, and an integer string too wide to fit -- with a TypeError,
// not a deprecation, so this half is php-exact rather than a §10 divergence.
// PHL only asked the question in the ~30 builtins that called the shared helper
// from their own body, so the rest narrowed silently: dechex(1e19) answered the
// hex of PHP_INT_MIN and array_fill(1e19,1,0) filled from it.
function zppIntCase(string $label, callable $fn): void {
    try {
        $r = $fn();
        echo $label, ' => NO-THROW ', var_export($r, true), "\n";
    } catch (\Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
foreach ([1e19, -1e19, 1e30, NAN, INF, -INF] as $zppIntV) {
    zppIntCase('dechex', fn() => dechex($zppIntV));
}
zppIntCase('decbin', fn() => decbin(1e19));
zppIntCase('decoct', fn() => decoct(1e19));
zppIntCase('array_fill#1', fn() => array_fill(1e19, 1, 'v'));
zppIntCase('array_unique', fn() => array_unique([1, 2], 1e19));
zppIntCase('explode', fn() => explode(',', 'a,b', 1e19));
zppIntCase('strpos', fn() => strpos('abc', 'c', 1e19));
zppIntCase('substr_count', fn() => substr_count('abc', 'a', 1e19));
zppIntCase('mb_substr', fn() => mb_substr('abc', 1e19));
zppIntCase('count', fn() => count([1], 1e19));
zppIntCase('intdiv#2', fn() => intdiv(1, 1e19));
zppIntCase('json_encode', fn() => json_encode([1], 1e19));

// The same question for a numeric STRING: php reaches both through one ZPP macro
// and words the refusal the same way.
zppIntCase('dechex "1e19"', fn() => dechex('1e19'));
zppIntCase('dechex wide-int', fn() => dechex('99999999999999999999'));
zppIntCase('str_repeat wide', fn() => str_repeat('a', '99999999999999999999'));
zppIntCase('array_fill wide', fn() => array_fill('99999999999999999999', 1, 'v'));

// ...and what an int CAN hold still goes through, from either shape.
zppIntCase('dechex int', fn() => dechex(255));
zppIntCase('dechex whole float', fn() => dechex(255.0));
zppIntCase('dechex int-string', fn() => dechex('255'));
zppIntCase('dechex INT_MIN', fn() => dechex(PHP_INT_MIN));
zppIntCase('array_slice num-str', fn() => array_slice([1, 2, 3], '1'));
--EXPECT--
dechex => TypeError: dechex(): Argument #1 ($num) must be of type int, float given
dechex => TypeError: dechex(): Argument #1 ($num) must be of type int, float given
dechex => TypeError: dechex(): Argument #1 ($num) must be of type int, float given
dechex => TypeError: dechex(): Argument #1 ($num) must be of type int, float given
dechex => TypeError: dechex(): Argument #1 ($num) must be of type int, float given
dechex => TypeError: dechex(): Argument #1 ($num) must be of type int, float given
decbin => TypeError: decbin(): Argument #1 ($num) must be of type int, float given
decoct => TypeError: decoct(): Argument #1 ($num) must be of type int, float given
array_fill#1 => TypeError: array_fill(): Argument #1 ($start_index) must be of type int, float given
array_unique => TypeError: array_unique(): Argument #2 ($flags) must be of type int, float given
explode => TypeError: explode(): Argument #3 ($limit) must be of type int, float given
strpos => TypeError: strpos(): Argument #3 ($offset) must be of type int, float given
substr_count => TypeError: substr_count(): Argument #3 ($offset) must be of type int, float given
mb_substr => TypeError: mb_substr(): Argument #2 ($start) must be of type int, float given
count => TypeError: count(): Argument #2 ($mode) must be of type int, float given
intdiv#2 => TypeError: intdiv(): Argument #2 ($num2) must be of type int, float given
json_encode => TypeError: json_encode(): Argument #2 ($flags) must be of type int, float given
dechex "1e19" => TypeError: dechex(): Argument #1 ($num) must be of type int, string given
dechex wide-int => TypeError: dechex(): Argument #1 ($num) must be of type int, string given
str_repeat wide => TypeError: str_repeat(): Argument #2 ($times) must be of type int, string given
array_fill wide => TypeError: array_fill(): Argument #1 ($start_index) must be of type int, string given
dechex int => NO-THROW 'ff'
dechex whole float => NO-THROW 'ff'
dechex int-string => NO-THROW 'ff'
dechex INT_MIN => NO-THROW '8000000000000000'
array_slice num-str => NO-THROW array (
  0 => 2,
  1 => 3,
)
--CLEAN--
<?php
