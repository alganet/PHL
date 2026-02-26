--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_assoc should handle integer keys as well as strings
--FILE--
<?php
$a = array('a' => 1, 0 => 0, 'b' => 2, 'c' => 3);
$b = array('b' => 2, 0 => 0);
$d = array_diff_assoc($a, $b);
echo implode(',', array_keys($d)) . PHP_EOL; // expecting 'a,c'
?>
--EXPECT--
a,c
--CLEAN--
<?php
unset($a, $b, $d);
