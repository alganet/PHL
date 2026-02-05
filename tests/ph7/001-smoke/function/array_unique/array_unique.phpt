--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique should remove duplicate values while preserving first keys
--FILE--
<?php
$in = array('a'=>1,'b'=>2,'c'=>1,'d'=>2,'e'=>3);
$u = array_unique($in);
echo implode(',', array_keys($u)) . PHP_EOL; // a,b,e
?>
--EXPECT--
a,b,e
--CLEAN--
<?php
unset($in, $u);
