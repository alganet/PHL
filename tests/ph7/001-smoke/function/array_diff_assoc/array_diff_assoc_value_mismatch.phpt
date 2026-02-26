--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Entries with same key but different value should be kept
--FILE--
<?php
$a = array('k' => 1, 'l' => 2);
$b = array('k' => 0);
$d = array_diff_assoc($a, $b);
echo implode(',', array_keys($d)) . PHP_EOL; // expecting 'k,l'
?>
--EXPECT--
k,l
--CLEAN--
<?php
unset($a, $b, $d);
