--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: unset array element does not affect copy
--FILE--
<?php
$a = [10, 20, 30];
$b = $a;
unset($b[1]);
echo count($a) . "\n";
echo $a[1] . "\n";
echo count($b) . "\n";
?>
--EXPECT--
3
20
2
--CLEAN--
<?php
unset($a, $b);
