--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: repeated writes to separated array do not re-separate
--FILE--
<?php
$a = [1, 2, 3];
$b = $a;
// First write COW-separates; subsequent writes must not re-copy
$b[0] = 10;
$b[1] = 20;
$b[2] = 30;
echo $a[0] . " " . $a[1] . " " . $a[2] . "\n";
echo $b[0] . " " . $b[1] . " " . $b[2] . "\n";
?>
--EXPECT--
1 2 3
10 20 30
--CLEAN--
<?php
unset($a, $b);
