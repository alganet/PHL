--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with length that does not divide evenly has shorter last chunk
--FILE--
<?php
$r = str_split("hello", 2);
echo $r[0] . ":" . $r[1] . ":" . $r[2] . PHP_EOL;
?>
--EXPECT--
he:ll:o
--CLEAN--
<?php
unset($r);
