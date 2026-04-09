--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Symmetric array destructuring basic assignment
--FILE--
<?php
[$a, $b] = [1, 2];
echo "$a $b\n";

[$x, $y, $z] = [10, 20, 30];
echo "$x $y $z\n";

$arr = [100, 200, 300];
[$p, $q, $r] = $arr;
echo "$p $q $r\n";
?>
--EXPECT--
1 2
10 20 30
100 200 300
--CLEAN--
<?php
unset($a, $b, $x, $y, $z, $arr, $p, $q, $r);
