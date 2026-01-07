--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Parse error missing semicolon
--SKIPIF--
<?php
if (function_exists('zend_version')) echo 'skip';
?>
--FILE--
<?php
echo "hello"
?>
--EXPECT--
hello