--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Double quoted string NUL escape
--FILE--
<?php
$s = "hello\0world";
echo strlen($s) . "\n";
echo bin2hex($s) . "\n";
?>
--EXPECT--
11
68656c6c6f00776f726c64
--CLEAN--
<?php
unset($s);
