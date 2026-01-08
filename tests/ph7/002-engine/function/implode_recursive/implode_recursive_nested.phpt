--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode_recursive with nested arrays
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test implode_recursive with nested arrays
$result = implode_recursive('-', array('a', array('b', 'c'), 'd'));
echo $result . "\n";
?>
--EXPECT--
a-b-c-d