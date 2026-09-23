--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE: count_chars() rejects a null argument and a lossy $mode — php deprecates and coerces, in the _zend twin
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// While count_chars() was embedded PHP its parameters were untyped, so null
// reached a (string)/(int) cast and answered. It declares php's
// `string $string, int $mode` now, and §10 turns php's two deprecations here
// into the TypeError php will eventually raise.
foreach ([
    'null_string' => static fn() => count_chars(null, 3),
    'null_mode'   => static fn() => count_chars("aab", null),
    'float_mode'  => static fn() => count_chars("aab", 1.5),
    'fstr_mode'   => static fn() => count_chars("aab", "1.5"),
] as $ccnName => $ccnFn) {
    try {
        $ccnFn();
        echo $ccnName, ": NO THROW\n";
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
// What php coerces without deprecating still works: a whole float, a whole
// float-string, an integer string and a bool.
var_dump(count_chars("aab", 1.0) === count_chars("aab", 1));
var_dump(count_chars("aab", "1.0") === count_chars("aab", 1));
var_dump(count_chars("aab", "1") === count_chars("aab", 1));
var_dump(count_chars("aab", true) === count_chars("aab", 1));
?>
--EXPECT--
count_chars(): Argument #1 ($string) must be of type string, null given
count_chars(): Argument #2 ($mode) must be of type int, null given
count_chars(): Argument #2 ($mode) must be of type int, float given
count_chars(): Argument #2 ($mode) must be of type int, string given
bool(true)
bool(true)
bool(true)
bool(true)
--CLEAN--
<?php
