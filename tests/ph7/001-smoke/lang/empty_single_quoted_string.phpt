--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Empty single-quoted strings
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test empty single-quoted strings
// Covers PH7_CompileSimpleString empty string handling

$empty1 = '';
$empty2 = "";

echo "Empty single: '$empty1'\n";
echo "Empty double: '$empty2'\n";

$concat = 'prefix' . '' . 'suffix';
echo "Concat with empty: '$concat'\n";

echo "Done\n";
?>
--EXPECT--
Empty single: ''
Empty double: ''
Concat with empty: 'prefixsuffix'
Done
--CLEAN--
<?php
unset($empty1, $empty2, $concat);
