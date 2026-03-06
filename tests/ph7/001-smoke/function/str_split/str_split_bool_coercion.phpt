--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with boolean true coerces to string "1" and splits
--FILE--
<?php
$r = str_split(true);
echo $r[0] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
1
1
--CLEAN--
<?php
unset($r);
