--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: basic single-argument expression
--FILE--
<?php
$f = fn($x) => $x * 2;
echo $f(5), "\n";
?>
--EXPECT--
10
--CLEAN--
<?php
unset($f);
