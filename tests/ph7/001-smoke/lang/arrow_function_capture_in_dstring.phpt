--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: captures variables referenced inside double-quoted strings
--FILE--
<?php
$name = "world";
$n = 42;
$f = fn() => "hello $name, answer is $n";
echo $f(), "\n";
?>
--EXPECT--
hello world, answer is 42
--CLEAN--
<?php
unset($f, $name, $n);
