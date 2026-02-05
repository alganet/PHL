--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_getcsv parses a CSV string and returns an array
--FILE--
<?php
$arr = str_getcsv("a,b,c", ',', '"', '\\');
echo count($arr) . "\n";
echo implode(':', $arr) . "\n";
?>
--EXPECT--
3
a:b:c
--CLEAN--
<?php
unset($arr);
