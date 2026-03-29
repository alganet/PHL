--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: as return value and in ternary
--FILE--
<?php
function getList() {
    return [10, 20, 30];
}
$a = getList();
echo $a[0], "\n";
echo $a[2], "\n";

$x = true ? [1, 2] : [3, 4];
echo $x[0], "\n";
$y = false ? [1, 2] : [3, 4];
echo $y[0], "\n";
?>
--EXPECT--
10
30
1
3
--CLEAN--
<?php
