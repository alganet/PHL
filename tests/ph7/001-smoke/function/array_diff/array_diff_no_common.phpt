--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff with no common values returns all from first array
--FILE--
<?php
$a = array(1, 2, 3);
$b = array(4, 5, 6);
$c = array_diff($a, $b);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
0:1,1:2,2:3,
--CLEAN--
<?php
unset($a, $b, $c);
