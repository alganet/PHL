--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Comma operator in expressions
--FILE--
<?php
$result = ($a = 1, $b = 2, $a + $b);
echo $result . "\n";
?>
--EXPECT--
3