--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine should assign correct values
--FILE--
<?php
$keys = array('a','b','c');
$vals = array(1,2,3);
$c = array_combine($keys,$vals);
// only check values order
echo implode(',', array_values($c)) . PHP_EOL;
?>
--EXPECT--
1,2,3
--CLEAN--
<?php
unset($keys, $vals, $c);
