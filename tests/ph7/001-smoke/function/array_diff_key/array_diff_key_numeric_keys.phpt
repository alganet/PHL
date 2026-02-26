--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_key should ignore values when comparing integer keys
--FILE--
<?php
$a = array(0 => 'zero', 1 => 'one', 2 => 'two');
$b = array(1 => 'uno', 3 => 'three');
$r = array_diff_key($a, $b);
echo implode(',', array_keys($r)) . PHP_EOL; // expecting '0,2'
?>
--EXPECT--
0,2
--CLEAN--
<?php
unset($a, $b, $r);
