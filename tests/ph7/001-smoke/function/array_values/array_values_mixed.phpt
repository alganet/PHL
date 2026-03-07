--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values returns values from mixed key types
--FILE--
<?php
$a = array(0 => 'zero', 'key' => 'val', 1 => 'one');
$v = array_values($a);
echo $v[0] . ',' . $v[1] . ',' . $v[2];
?>
--EXPECT--
zero,val,one
--CLEAN--
<?php
unset($a, $v);
