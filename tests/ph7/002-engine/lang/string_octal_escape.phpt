--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
String octal escape
--FILE--
<?php
echo "\o77";
echo "\n";
echo "\o10";
echo "\n";
?>
--EXPECT--
?
