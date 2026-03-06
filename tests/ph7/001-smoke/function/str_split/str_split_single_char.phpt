--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with single character string returns array with one element
--FILE--
<?php
$r = str_split("x");
echo $r[0] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
x
1
--CLEAN--
<?php
unset($r);
