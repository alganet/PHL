--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int returns the bound when min equals max
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
echo (random_int(7, 7) === 7) ? "eq_ok\n" : "eq_fail\n";
echo (random_int(-3, -3) === -3) ? "eq_neg_ok\n" : "eq_neg_fail\n";
?>
--EXPECT--
eq_ok
eq_neg_ok
--CLEAN--
<?php
