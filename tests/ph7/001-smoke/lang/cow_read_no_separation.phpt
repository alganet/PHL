--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: reading shared array elements does not destroy sharing
--FILE--
<?php
$a = [1, 2, 3];
$b = $a;
// Multiple reads should NOT separate — both still share
echo $b[0] . "\n";
echo $b[1] . "\n";
echo $b[2] . "\n";
// Mutation after reads should still COW-separate correctly
$b[0] = 99;
echo $a[0] . "\n";
echo $b[0] . "\n";
?>
--EXPECT--
1
2
3
1
99
--CLEAN--
<?php
unset($a, $b);
