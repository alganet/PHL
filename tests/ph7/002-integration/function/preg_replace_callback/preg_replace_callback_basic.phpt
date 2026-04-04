--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace_callback with closure
--FILE--
<?php
$r = preg_replace_callback('/\w+/', function($m) {
    return strtoupper($m[0]);
}, 'hello world');
echo $r . "\n";
?>
--EXPECT--
HELLO WORLD
--CLEAN--
<?php

