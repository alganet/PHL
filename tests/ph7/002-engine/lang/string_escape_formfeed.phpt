--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Double quoted string form feed escape sequence
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$str = "\f";
echo "Length: " . strlen($str) . "\n";
echo "Char code: " . ord($str) . "\n";
?>
--EXPECT--
Length: 1
Char code: 12