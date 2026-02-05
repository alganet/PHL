--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
double quoted string with invalid escape sequence
--FILE--
<?php
echo "hello \z world\n";
?>
--EXPECT--
hello z world
--CLEAN--
<?php

