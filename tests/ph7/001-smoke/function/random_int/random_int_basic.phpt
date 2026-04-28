--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int returns int values uniformly inside the requested closed range
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
$ok = true;
for ($i = 0; $i < 50; $i++) {
    $v = random_int(0, 10);
    if (!is_int($v) || $v < 0 || $v > 10) { $ok = false; break; }
}
echo $ok ? "range_ok\n" : "range_fail\n";
?>
--EXPECT--
range_ok
--CLEAN--
<?php
