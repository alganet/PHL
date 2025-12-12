--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode complex arrays and objects
--SKIPIF--
<?php if (!function_exists('json_encode') || !function_exists('json_decode')) { die('skip'); } ?>
--FILE--
<?php
$arr = array(1, 2, 3);
$str = json_encode($arr);
$decoded = json_decode($str, true);
echo (is_array($decoded) && count($decoded) === 3 && $decoded[0] == 1 ? "ok\n" : "fail\n");
echo is_array($decoded) ? "array\n" : "notarray\n";
?>
--CLEAN--
<?php
unset($str, $arr, $decoded);
?>
--EXPECT--
ok
array
