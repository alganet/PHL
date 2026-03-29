--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: nested arrays
--FILE--
<?php
$a = [1, [2, 3], [4, [5, 6]]];
echo $a[0], "\n";
echo $a[1][0], "\n";
echo $a[1][1], "\n";
echo $a[2][0], "\n";
echo $a[2][1][0], "\n";
echo $a[2][1][1], "\n";
?>
--EXPECT--
1
2
3
4
5
6
--CLEAN--
<?php
