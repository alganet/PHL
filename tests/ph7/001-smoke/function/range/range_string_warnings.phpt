--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
range() string-endpoint warnings and null deprecations match PHP 8
--FILE--
<?php
$restore = set_error_handler(function ($errno, $errstr) {
    echo "[$errno] $errstr\n";
    return true;
});
echo var_export(range('a', 5), true), "\n";
echo var_export(range(5, 'a'), true), "\n";
echo var_export(range('aa', 'e'), true), "\n";
echo var_export(range('a', 'ee'), true), "\n";
echo var_export(range('', 'e'), true), "\n";
echo var_export(range('a', ''), true), "\n";
echo var_export(range('a', 'z', 0.5), true), "\n";
echo var_export(range('1', '5', 0.5), true), "\n";
echo var_export(range('10', 'c'), true), "\n";
echo var_export(range('c', '10'), true), "\n";
echo var_export(range('0x1A', '30') === range(0, 30), true), "\n";
echo var_export(range(null, 2), true), "\n";
echo var_export(range(2, null), true), "\n";
try { range(1, 5, null); } catch (\ValueError $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
set_error_handler($restore);
--EXPECT--
[2] range(): Argument #2 ($end) must be a single byte string if argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0
array (
  0 => 0,
  1 => 1,
  2 => 2,
  3 => 3,
  4 => 4,
  5 => 5,
)
[2] range(): Argument #1 ($start) must be a single byte string if argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0
array (
  0 => 5,
  1 => 4,
  2 => 3,
  3 => 2,
  4 => 1,
  5 => 0,
)
[2] range(): Argument #1 ($start) must be a single byte, subsequent bytes are ignored
array (
  0 => 'a',
  1 => 'b',
  2 => 'c',
  3 => 'd',
  4 => 'e',
)
[2] range(): Argument #2 ($end) must be a single byte, subsequent bytes are ignored
array (
  0 => 'a',
  1 => 'b',
  2 => 'c',
  3 => 'd',
  4 => 'e',
)
[2] range(): Argument #1 ($start) must not be empty, casted to 0
[2] range(): Argument #1 ($start) must be a single byte string if argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0
array (
  0 => 0,
)
[2] range(): Argument #2 ($end) must not be empty, casted to 0
[2] range(): Argument #2 ($end) must be a single byte string if argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0
array (
  0 => 0,
)
[2] range(): Argument #3 ($step) must be of type int when generating an array of characters, inputs converted to 0
array (
  0 => 0.0,
)
array (
  0 => 1.0,
  1 => 1.5,
  2 => 2.0,
  3 => 2.5,
  4 => 3.0,
  5 => 3.5,
  6 => 4.0,
  7 => 4.5,
  8 => 5.0,
)
[2] range(): Argument #1 ($start) must be a single byte string if argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0
array (
  0 => 10,
  1 => 9,
  2 => 8,
  3 => 7,
  4 => 6,
  5 => 5,
  6 => 4,
  7 => 3,
  8 => 2,
  9 => 1,
  10 => 0,
)
[2] range(): Argument #2 ($end) must be a single byte string if argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0
array (
  0 => 0,
  1 => 1,
  2 => 2,
  3 => 3,
  4 => 4,
  5 => 5,
  6 => 6,
  7 => 7,
  8 => 8,
  9 => 9,
  10 => 10,
)
[2] range(): Argument #1 ($start) must be a single byte, subsequent bytes are ignored
[2] range(): Argument #2 ($end) must be a single byte string if argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0
true
[8192] range(): Passing null to parameter #1 ($start) of type string|int|float is deprecated
array (
  0 => 0,
  1 => 1,
  2 => 2,
)
[8192] range(): Passing null to parameter #2 ($end) of type string|int|float is deprecated
array (
  0 => 2,
  1 => 1,
  2 => 0,
)
[8192] range(): Passing null to parameter #3 ($step) of type int|float is deprecated
ValueError: range(): Argument #3 ($step) cannot be 0
--CLEAN--
<?php
unset($restore);
