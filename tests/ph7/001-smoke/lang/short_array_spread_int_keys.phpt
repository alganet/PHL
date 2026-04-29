--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread (...) renumbers integer keys
--FILE--
<?php
$a = [10, 20, 30];
$b = [1, ...$a, 99];
echo count($b), "\n";
foreach ($b as $k => $v) {
    echo $k, "=", $v, "\n";
}
?>
--EXPECT--
5
0=1
1=10
2=20
3=30
4=99
--CLEAN--
<?php
unset($a, $b);
