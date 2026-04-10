--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: multiple parameters
--FILE--
<?php
$f = fn($x, $y) => $x + $y;
echo $f(3, 4), "\n";
?>
--EXPECT--
7
--CLEAN--
<?php
unset($f);
