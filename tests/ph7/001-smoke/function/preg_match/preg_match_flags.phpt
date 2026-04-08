--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match with case-insensitive and multiline flags
--FILE--
<?php
echo preg_match('/hello/i', 'Hello World') . "\n";
echo preg_match('/^world/im', "Hello\nWorld") . "\n";
echo preg_match('/hello.world/si', "Hello\nWorld") . "\n";
?>
--EXPECT--
1
1
1
--CLEAN--
<?php

