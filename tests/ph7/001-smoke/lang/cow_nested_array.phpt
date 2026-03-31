--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: nested array write does not affect original
--FILE--
<?php
$a = [[1, 2], [3, 4]];
$b = $a;
$b[0][0] = 99;
echo $a[0][0] . "\n";
echo $b[0][0] . "\n";
?>
--EXPECT--
1
99
--CLEAN--
<?php
unset($a, $b);
