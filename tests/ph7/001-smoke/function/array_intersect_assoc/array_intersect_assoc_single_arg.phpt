--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc with single argument returns that array
--FILE--
<?php
$a = array(1, 2, 3);
$c = array_intersect_assoc($a);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
0:1,1:2,2:3,
--CLEAN--
<?php
unset($a, $c);
