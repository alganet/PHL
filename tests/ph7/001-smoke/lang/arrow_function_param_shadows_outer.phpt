--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: parameter shadows outer variable of same name
--FILE--
<?php
$x = 99;
$f = fn($x) => $x * 2;
echo $f(5), "\n";
echo $x, "\n";
?>
--EXPECT--
10
99
--CLEAN--
<?php
unset($x, $f);
