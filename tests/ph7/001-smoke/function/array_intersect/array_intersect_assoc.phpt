--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc compares both key and value for intersection
--FILE--
<?php
$a = array('a'=>1, 'b'=>2, 'c'=>3);
$b = array('a'=>1, 'b'=>99, 'd'=>3);
$c = array_intersect_assoc($a, $b);
echo implode(',', array_keys($c)) . PHP_EOL; // a
?>
--EXPECT--
a
--CLEAN--
<?php
unset($a, $b, $c);
