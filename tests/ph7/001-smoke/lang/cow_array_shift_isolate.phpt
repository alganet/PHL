--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: array_shift does not affect copy
--FILE--
<?php
$a = [1, 2, 3];
$b = $a;
array_shift($a);
echo count($a) . "\n";
echo count($b) . "\n";
?>
--EXPECT--
2
3
--CLEAN--
<?php
unset($a, $b);
