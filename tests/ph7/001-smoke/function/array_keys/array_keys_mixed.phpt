--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys returns both integer and string keys
--FILE--
<?php
$a = array(0 => 'a', 'name' => 'b', 1 => 'c');
$k = array_keys($a);
echo $k[0] . ',' . $k[1] . ',' . $k[2];
?>
--EXPECT--
0,name,1
--CLEAN--
<?php
unset($a, $k);
