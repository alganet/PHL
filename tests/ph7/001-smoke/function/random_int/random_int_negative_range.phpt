--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int respects bounds when both min and max are negative
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
$ok = true;
for ($i = 0; $i < 30; $i++) {
    $v = random_int(-5, -1);
    if (!is_int($v) || $v < -5 || $v > -1) { $ok = false; break; }
}
echo $ok ? "neg_ok\n" : "neg_fail\n";
?>
--EXPECT--
neg_ok
--CLEAN--
<?php
