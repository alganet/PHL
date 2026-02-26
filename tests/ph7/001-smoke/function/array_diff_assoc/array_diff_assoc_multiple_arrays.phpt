--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_assoc can diff against more than two arrays
--FILE--
<?php
$a = array(1 => 1, 2 => 2, 3 => 3);
$b = array(1 => 1);
$c = array(3 => 3);
$d = array_diff_assoc($a, $b, $c);
echo implode(',', array_keys($d)) . PHP_EOL; // expecting '2'
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a, $b, $c, $d);
