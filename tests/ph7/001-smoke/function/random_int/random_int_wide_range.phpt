--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int handles a range wider than 32 bits
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
$min = 0;
$max = 9999999999; // 10^10, exceeds 2^32, exercises the 64-bit code path
$v = random_int($min, $max);
echo (is_int($v) && $v >= $min && $v <= $max) ? "wide_ok\n" : "wide_fail\n";
?>
--EXPECT--
wide_ok
--CLEAN--
<?php
