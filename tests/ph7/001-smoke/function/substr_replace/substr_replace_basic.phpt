--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_replace scalar form: offset/length normalization matches php
--FILE--
<?php
echo substr_replace("Hello", "World", 0), "\n";
echo substr_replace("Hello", "World", 1, 2), "\n";
echo substr_replace("Hello", "X", -2, 1), "\n";
echo substr_replace("Hello", "X", 10, 5), "\n";
echo substr_replace("Hello", "X", 2, -1), "\n";
echo substr_replace("Hello", "X", -10, 2), "\n";
echo substr_replace("Hello", "X", 2, null), "\n";
echo substr_replace("", "abc", 0), "\n";
echo substr_replace("abc", "", 1, 1), "\n";
echo substr_replace("Hello", "X", PHP_INT_MAX, PHP_INT_MAX), "\n";
echo substr_replace("Hello", "X", PHP_INT_MIN, PHP_INT_MIN), "\n";
echo substr_replace("Hello", "X", 2, PHP_INT_MAX), "\n";
echo substr_replace("Hello", "X", -3, PHP_INT_MIN), "\n";
// scalar string + array replace uses the first element ("" when empty)
echo substr_replace("hello", ["X", "Y"], 1), "\n";
echo substr_replace("hello", [], 1), "\n";
// int subject is stringified
echo substr_replace(12345, "X", 1, 2), "\n";
?>
--EXPECT--
World
HWorldlo
HelXo
HelloX
HeXo
Xllo
HeX
abc
ac
HelloX
XHello
HeX
HeXllo
hX
h
1X45
--CLEAN--
<?php
