--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: array_push does not affect copy
--FILE--
<?php
$a = [1, 2];
$b = $a;
array_push($a, 3);
echo count($a) . "\n";
echo count($b) . "\n";
?>
--EXPECT--
3
2
--CLEAN--
<?php
unset($a, $b);
