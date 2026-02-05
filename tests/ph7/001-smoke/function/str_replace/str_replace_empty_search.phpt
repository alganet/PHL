--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace with empty search string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test str_replace with empty search string
// This should trigger early return/error handling
$result = str_replace('', 'replacement', 'test string');
echo $result . "\n";
?>
--EXPECT--
test string
--CLEAN--
<?php
unset($result);
