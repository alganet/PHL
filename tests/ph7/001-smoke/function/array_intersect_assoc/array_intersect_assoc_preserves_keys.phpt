--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc preserves keys from the first array
--FILE--
<?php
$a = array(10 => "x", 20 => "y", 30 => "z");
$b = array(10 => "x", 20 => "y", 30 => "z");
$c = array_intersect_assoc($a, $b);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
10:x,20:y,30:z,
--CLEAN--
<?php
unset($a, $b, $c);
