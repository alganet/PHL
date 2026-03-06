--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with length equal to string length returns whole string as single element
--FILE--
<?php
$r = str_split("hello", 5);
echo $r[0] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
hello
1
--CLEAN--
<?php
unset($r);
