--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff compares string values correctly
--FILE--
<?php
$a = array('apple', 'banana', 'cherry');
$b = array('banana', 'date');
$c = array_diff($a, $b);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
0:apple,2:cherry,
--CLEAN--
<?php
unset($a, $b, $c);
