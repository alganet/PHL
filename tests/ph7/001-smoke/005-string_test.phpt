--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String operations
--FILE--
<?php
$a = "Hello";
$b = "World";
echo $a . " " . $b; // Hello World
echo "\n";
echo strlen($a); // 5
echo "\n";
echo strlen($a . $b); // 10
?>
--EXPECT--
Hello World
5
10
--CLEAN--
<?php
unset($a, $b);
?>