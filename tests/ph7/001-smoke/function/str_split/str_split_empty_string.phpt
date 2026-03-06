--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with empty string returns empty array
--FILE--
<?php
$r = str_split("");
echo is_array($r) ? "array" : "not_array" ;
echo PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
array
0
--CLEAN--
<?php
unset($r);
