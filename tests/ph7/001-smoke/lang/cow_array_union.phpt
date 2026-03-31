--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: array union (+= ) does not affect copy
--FILE--
<?php
$a = [1, 2, 3];
$b = $a;
$a += [3 => 99, 4 => 100];
echo count($a) . "\n";
echo count($b) . "\n";
echo $a[4] . "\n";
?>
--EXPECT--
5
3
100
--CLEAN--
<?php
unset($a, $b);
