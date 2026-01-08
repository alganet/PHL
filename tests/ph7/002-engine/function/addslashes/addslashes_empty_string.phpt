--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: addslashes with empty string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test addslashes with empty string
$result = addslashes('');
echo "Empty string result: '" . $result . "'\n";
?>
--EXPECT--
Empty string result: ''