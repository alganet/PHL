--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: typed parameter
--FILE--
<?php
$f = fn(int $x): int => $x + 1;
echo $f(5), "\n";
echo $f(0), "\n";
?>
--EXPECT--
6
1
--CLEAN--
<?php
unset($f);
