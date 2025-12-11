--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice should remove elements and return removed chunk, and insert replacements
--FILE--
<?php
$a = array(1,2,3,4,5);
$removed = array_splice($a, 1, 2, array(9,10));
echo implode(',', $removed) . PHP_EOL; // 2,3
echo implode(',', $a) . PHP_EOL; // 1,9,10,4,5
?>
--EXPECT--
2,3
1,9,10,4,5
--CLEAN--
<?php
unset($a,$removed);
?>
