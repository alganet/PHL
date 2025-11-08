--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Type checking functions
--FILE--
<?php
$int = 42;
$string = "hello";
$array = array(1,2,3);
$bool = true;
$null = null;

echo is_int($int) ? "yes" : "no"; // yes
echo "\n";
echo is_string($string) ? "yes" : "no"; // yes
echo "\n";
echo is_array($array) ? "yes" : "no"; // yes
echo "\n";
echo is_bool($bool) ? "yes" : "no"; // yes
echo "\n";
echo is_null($null) ? "yes" : "no"; // yes
?>
--EXPECT--
yes
yes
yes
yes
yes
--CLEAN--
<?php
unset($int, $string, $array, $bool, $null);
?>