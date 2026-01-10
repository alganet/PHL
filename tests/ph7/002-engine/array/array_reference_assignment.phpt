--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array assignment by reference
--FILE--
<?php
$a = 1;
$b = 2;
$arr = array(&$a, &$b);
$a = 10;
$b = 20;
echo $arr[0] . ',' . $arr[1] . "\n";
?>
--EXPECT--
10,20