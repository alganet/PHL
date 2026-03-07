--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values returns values from associative array
--FILE--
<?php
$a = array('x' => 'a', 'y' => 'b');
$v = array_values($a);
echo $v[0] . ',' . $v[1];
?>
--EXPECT--
a,b
--CLEAN--
<?php
unset($a, $v);
