--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_key with more than two arrays only keeps entries whose keys
are absent from all others
--FILE--
<?php
$a = array('a'=>1,'b'=>2,'c'=>3);
$b = array('a'=>100);
$c = array('c'=>300);
$r = array_diff_key($a, $b, $c);
echo implode(',', array_keys($r)) . PHP_EOL; // expecting 'b'
?>
--EXPECT--
b
--CLEAN--
<?php
unset($a, $b, $c, $r);
