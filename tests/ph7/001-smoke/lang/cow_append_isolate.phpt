--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: append to copy does not affect original
--FILE--
<?php
$a = [1, 2];
$b = $a;
$b[] = 3;
echo count($a) . "\n";
echo count($b) . "\n";
?>
--EXPECT--
2
3
--CLEAN--
<?php
unset($a, $b);
