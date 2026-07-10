--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
range() throws PHP 8 ValueError/TypeError/ArgumentCountError on domain and type errors
--FILE--
<?php
function probe_range(...$args) {
    try {
        $r = range(...$args);
        echo 'ok: ', str_replace("\n", '', var_export($r, true)), "\n";
    } catch (\Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
probe_range(1, 10, 0);
probe_range(1, 10, -2);
probe_range('a', 'z', -3);
probe_range(1, 5, 10);
probe_range(5, 1, 10);
probe_range('a', 'b', 5);
probe_range(1, 5, INF);
probe_range(1, 5, NAN);
probe_range(1, NAN);
probe_range(NAN, 5);
probe_range(INF, 5);
probe_range(1, INF);
probe_range(NAN, 5, 0);
probe_range([1], 5);
probe_range([1], 5, 0);
probe_range(new stdClass, 5);
probe_range(1, 5, [1]);
probe_range(1, 5, 'abc');
probe_range(5, 1, PHP_INT_MIN);
probe_range(0, PHP_INT_MAX);
probe_range(PHP_INT_MAX, 0);
probe_range(0, 1073741823);
probe_range(0, 1, 1e-10);
probe_range(0, 1e18, 0.5);
probe_range(1e18, 0, 0.5);
probe_range(1, 5, 2, 9);
probe_range(1, 5, 1e19);
--EXPECT--
ValueError: range(): Argument #3 ($step) cannot be 0
ValueError: range(): Argument #3 ($step) must be greater than 0 for increasing ranges
ValueError: range(): Argument #3 ($step) must be greater than 0 for increasing ranges
ValueError: range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)
ValueError: range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)
ValueError: range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)
ValueError: range(): Argument #3 ($step) must be a finite number, INF provided
ValueError: range(): Argument #3 ($step) must be a finite number, NAN provided
ValueError: range(): Argument #2 ($end) must be a finite number, NAN provided
ValueError: range(): Argument #1 ($start) must be a finite number, NAN provided
ValueError: range(): Argument #1 ($start) must be a finite number, INF provided
ValueError: range(): Argument #2 ($end) must be a finite number, INF provided
ValueError: range(): Argument #3 ($step) cannot be 0
TypeError: range(): Argument #1 ($start) must be of type string|int|float, array given
TypeError: range(): Argument #1 ($start) must be of type string|int|float, array given
TypeError: range(): Argument #1 ($start) must be of type string|int|float, stdClass given
TypeError: range(): Argument #3 ($step) must be of type int|float, array given
TypeError: range(): Argument #3 ($step) must be of type int|float, string given
ValueError: range(): Argument #3 ($step) must be greater than -9223372036854775808
ValueError: The supplied range exceeds the maximum array size by 9223372035781033984 elements: start=0, end=9223372036854775807, step=1. Calculated size: 9223372036854775807. Maximum size: 1073741824.
ValueError: The supplied range exceeds the maximum array size by 9223372035781033984 elements: start=0, end=9223372036854775807, step=1. Calculated size: 9223372036854775807. Maximum size: 1073741824.
ValueError: The supplied range exceeds the maximum array size by 0 elements: start=0, end=1073741823, step=1. Calculated size: 1073741823. Maximum size: 1073741824.
ValueError: The supplied range exceeds the maximum array size by 8926258177.0 elements: start=0.0, end=1.0, step=0.0. Max size: 1073741824
ValueError: The supplied range exceeds the maximum array size by 1999999998926258176.0 elements: start=0.0, end=1000000000000000000.0, step=0.5. Max size: 1073741824
ValueError: The supplied range exceeds the maximum array size by 1999999998926258176.0 elements: start=0.0, end=1000000000000000000.0, step=0.5. Max size: 1073741824
ArgumentCountError: range() expects at most 3 arguments, 4 given
ValueError: range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)
--CLEAN--
<?php
