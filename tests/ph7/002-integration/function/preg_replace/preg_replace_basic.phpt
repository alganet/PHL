--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace basic replacement and backreferences
--FILE--
<?php
echo preg_replace('/world/i', 'PHP', 'Hello World') . "\n";
echo preg_replace('/(\w+)\s(\w+)/', '$2 $1', 'Hello World') . "\n";
echo preg_replace('/\d+/', '', 'abc123def456') . "\n";
?>
--EXPECT--
Hello PHP
World Hello
abcdef
--CLEAN--
<?php

