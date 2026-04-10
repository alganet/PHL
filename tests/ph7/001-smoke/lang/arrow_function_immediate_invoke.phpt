--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: immediately invoked expression
--FILE--
<?php
echo (fn($x) => $x * 2)(5), "\n";
echo (fn() => "hello")(), "\n";
?>
--EXPECT--
10
hello
--CLEAN--
<?php
