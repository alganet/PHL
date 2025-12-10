--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rand generates random number
--SKIPIF--
<?php
if (!function_exists('rand')) { echo 'skip: rand not available'; }
?>
--FILE--
<?php
$val1 = rand();
$val2 = rand();
if (is_int($val1) && is_int($val2)) { echo "rand_int_ok\n"; } else { echo "rand_int_failed\n"; }
if ($val1 >= 0 && $val2 >= 0) { echo "rand_positive_ok\n"; } else { echo "rand_positive_failed\n"; }
$val3 = rand(10, 20);
if ($val3 >= 10 && $val3 <= 20) { echo "rand_range_ok\n"; } else { echo "rand_range_failed\n"; }
?>
--EXPECT--
rand_int_ok
rand_positive_ok
rand_range_ok
