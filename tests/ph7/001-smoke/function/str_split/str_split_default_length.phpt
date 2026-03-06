--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with default split length splits into single characters
--FILE--
<?php
$r = str_split("abc");
echo $r[0] . ":" . $r[1] . ":" . $r[2] . PHP_EOL;
?>
--EXPECT--
a:b:c
--CLEAN--
<?php
unset($r);
