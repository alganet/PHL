--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with length that divides evenly produces equal chunks
--FILE--
<?php
$r = str_split("abcdef", 2);
echo $r[0] . ":" . $r[1] . ":" . $r[2] . PHP_EOL;
?>
--EXPECT--
ab:cd:ef
--CLEAN--
<?php
unset($r);
