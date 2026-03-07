--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique removes duplicate values preserving first keys from associative array
--FILE--
<?php
$u = array_unique(array('a'=>1,'b'=>2,'c'=>1,'d'=>2,'e'=>3));
foreach($u as $k=>$v) echo "$k:$v ";
echo PHP_EOL;
?>
--EXPECT--
a:1 b:2 e:3
--CLEAN--
<?php
unset($u);
