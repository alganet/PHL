--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff with multiple arrays should only keep values absent from all others
--FILE--
<?php
$a = array(1,2,3);
$b = array(1);
$c = array(3);
$d = array_diff($a, $b, $c);
echo implode(',', $d) . PHP_EOL; // expecting '2'
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a, $b, $c, $d);
