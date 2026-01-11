--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with references
--FILE--
<?php
$a = 1;
$b = 2;
$arr = array(&$a, &$b);
$a = 10;
echo $arr[0] . ' ' . $arr[1] . "\n";
?>
--EXPECT--
10 2