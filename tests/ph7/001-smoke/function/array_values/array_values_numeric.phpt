--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values returns values from numeric array with reindexing
--FILE--
<?php
$a = array(5 => 'a', 10 => 'b', 15 => 'c');
$v = array_values($a);
echo $v[0] . ',' . $v[1] . ',' . $v[2];
?>
--EXPECT--
a,b,c
--CLEAN--
<?php
unset($a, $v);
