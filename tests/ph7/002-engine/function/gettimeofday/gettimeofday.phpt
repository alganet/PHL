--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gettimeofday array vs float variants
--SKIPIF--
<?php
if (!function_exists('gettimeofday')) { echo 'skip: gettimeofday not available'; }
?>
--FILE--
<?php
$arr = gettimeofday(false);
if (isset($arr['sec'])) { echo "sec_ok\n"; } else { echo "sec_missing\n"; }
if (isset($arr['usec'])) { echo "usec_ok\n"; } else { echo "usec_missing\n"; }
$float = gettimeofday(true);
if (is_float($float)) { echo "float_ok\n"; } else { echo "float_missing\n"; }
?>
--EXPECT--
sec_ok
usec_ok
float_ok
