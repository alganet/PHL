--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: unset element from original array preserves copy
--FILE--
<?php
$a = [10, 20, 30];
$b = $a;
unset($a[0]);
echo count($a) . "\n";
echo count($b) . "\n";
echo $b[0] . "\n";
?>
--EXPECT--
2
3
10
--CLEAN--
<?php
unset($a, $b);
