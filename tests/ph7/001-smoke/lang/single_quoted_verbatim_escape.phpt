--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Single quoted string verbatim escape
--FILE--
<?php
echo 'hello \ world' . "\n";
echo 'a\b\c' . "\n";
echo 'test\unknown' . "\n";
?>
--EXPECT--
hello \ world
a\b\c
test\unknown
--CLEAN--
<?php

