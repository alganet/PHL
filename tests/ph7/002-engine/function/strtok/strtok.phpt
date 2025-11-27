--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strtok returns tokens across calls
--FILE--
<?php
$str = "a,b,c";
$t = strtok($str, ',');
echo "first=" . $t . PHP_EOL;
$t = strtok(',');
echo "next1=" . $t . PHP_EOL;
$t = strtok(',');
echo "next2=" . $t . PHP_EOL;
?>
--EXPECT--
first=a
next1=b
next2=c
