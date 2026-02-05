--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Top-level constant declarations with complex expressions
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 builtin parser";
}
?>
--FILE--
<?php
const WELCOME = "Hello" . " " . "World";
const FIVE = 2 + 3;
const STR = strtoupper("ok");
echo WELCOME . "\n";
echo FIVE . "\n";
echo STR . "\n";
?>
--EXPECT--
Hello World
5
OK
--CLEAN--
<?php

