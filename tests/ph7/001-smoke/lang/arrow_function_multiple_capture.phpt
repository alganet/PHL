--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: captures multiple outer variables
--FILE--
<?php
$a = 1;
$b = 2;
$c = 3;
$f = fn() => $a + $b + $c;
echo $f(), "\n";
?>
--EXPECT--
6
--CLEAN--
<?php
unset($a, $b, $c, $f);
