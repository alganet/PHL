--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String functions
--FILE--
<?php
$string = "Hello World";
echo strpos($string, "World"); // 6
echo "\n";
echo substr($string, 6, 5); // World
echo "\n";
echo strtoupper($string); // HELLO WORLD
echo "\n";
echo strtolower($string); // hello world
echo "\n";
echo str_replace("World", "PHP", $string); // Hello PHP
?>
--EXPECT--
6
World
HELLO WORLD
hello world
Hello PHP
--CLEAN--
<?php
unset($string);
?>