--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation with array subscript access
--FILE--
<?php
$arr = array("first", "second", "third");
echo "Item: $arr[0]\n";
echo "Item: $arr[1]\n";
echo "Item: $arr[2]\n";
?>
--EXPECT--
Item: first
Item: second
Item: third
--CLEAN--
<?php
unset($arr);
