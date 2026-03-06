--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect with string values preserves keys from first array
--FILE--
<?php
$a = array('a' => 'green', 'b' => 'brown', 'c' => 'blue', 'd' => 'red');
$b = array('e' => 'green', 'f' => 'yellow', 'g' => 'red');
$r = array_intersect($a, $b);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
a:green,d:red,
--CLEAN--
<?php
unset($a, $b, $r);
