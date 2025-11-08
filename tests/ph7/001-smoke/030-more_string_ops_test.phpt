--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
More string operations
--FILE--
<?php
$string = "hello world";
$parts = explode(" ", $string);
echo count($parts); // 2
echo "\n";
echo $parts[0]; // hello
echo "\n";
$joined = implode("-", $parts);
echo $joined; // hello-world
?>
--EXPECT--
2
hello
hello-world
--CLEAN--
<?php
unset($string, $parts, $joined);
?>