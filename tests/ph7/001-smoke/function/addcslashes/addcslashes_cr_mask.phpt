--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes carriage return when mask contains CR
--FILE--
<?php
echo addcslashes("a\rb","\r") . "\n";
?>
--EXPECT--
a\rb
--CLEAN--
<?php


