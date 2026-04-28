--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int handles a wide range that straddles zero (exercises 64-bit + signed-add path)
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
$min = -9999999999;
$max =  9999999999;
$ok = true;
for ($i = 0; $i < 30; $i++) {
    $v = random_int($min, $max);
    if (!is_int($v) || $v < $min || $v > $max) { $ok = false; break; }
}
echo $ok ? "wide_neg_ok\n" : "wide_neg_fail\n";
?>
--EXPECT--
wide_neg_ok
--CLEAN--
<?php
