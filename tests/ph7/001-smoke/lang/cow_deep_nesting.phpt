--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: deep nesting (3 levels) write does not affect original
--FILE--
<?php
$a = [[[1]]];
$b = $a;
$b[0][0][0] = 99;
echo $a[0][0][0] . "\n";
echo $b[0][0][0] . "\n";
?>
--EXPECT--
1
99
--CLEAN--
<?php
unset($a, $b);
