--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match with named capture groups
--FILE--
<?php
$m = null;
preg_match('/(?P<first>\w+)\s(?P<second>\w+)/', 'Hello World', $m);
echo $m['first'] . "\n";
echo $m['second'] . "\n";
echo $m[1] . "\n";
echo $m[2] . "\n";
?>
--EXPECT--
Hello
World
Hello
World
--CLEAN--
<?php

