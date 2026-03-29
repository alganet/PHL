--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expression dereferencing: string literal subscript
--FILE--
<?php
echo "hello"[0], "\n";
echo "hello"[4], "\n";
$s = "world";
echo $s[0], "\n";
?>
--EXPECT--
h
o
w
--CLEAN--
<?php
