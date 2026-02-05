--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file() builtin reads lines into an array
--SKIPIF--
<?php if (!function_exists('file')) { die('skip'); } ?>
--FILE--
<?php
$tmp = tempnam(sys_get_temp_dir(), 'ph7_file_');
file_put_contents($tmp, "L1\nL2\n");
$arr = file($tmp);
echo count($arr) . "\n";
echo implode('|', array_map('rtrim', $arr)) . "\n";
?>
--EXPECT--
2
L1|L2
--CLEAN--
<?php
unlink($tmp);
unset($tmp, $arr);
