--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Array access with curly braces (converted to square brackets)
--FILE--
<?php
$a = array('key' => 'value');

echo "Testing array access with curly braces...\n";

// This should be converted to $a['key']
echo "Value: " . $a{'key'} . "\n";

echo "Test completed\n";
?>
--EXPECTF--
Testing array access with curly braces...
Value: value
Test completed
--CLEAN--
<?php
unset($a);
