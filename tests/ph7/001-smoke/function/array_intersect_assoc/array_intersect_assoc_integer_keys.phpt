--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc with integer keys matches by key and value
--FILE--
<?php
$a = array(0 => "a", 1 => "b", 2 => "c");
$b = array(0 => "a", 1 => "x", 2 => "c");
$c = array_intersect_assoc($a, $b);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
0:a,2:c,
--CLEAN--
<?php
unset($a, $b, $c);
