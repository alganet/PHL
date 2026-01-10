--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode with ENT_NOQUOTES flag for double quotes
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test html_entity_decode with ENT_NOQUOTES flag for double quotes
// ENT_NOQUOTES should prevent decoding of double quotes

// Test basic double quote with ENT_NOQUOTES (flag = 4)
$result1 = html_entity_decode('"Hello"', ENT_NOQUOTES);
echo ($result1 === '"Hello"') ? "NOQUOTES_DQ_OK\n" : "NOQUOTES_DQ_FAIL: '$result1'\n";

// Test double quote with ENT_COMPAT (default) - should decode
$result2 = html_entity_decode('"Hello"');
echo ($result2 === '"Hello"') ? "COMPAT_DQ_OK\n" : "COMPAT_DQ_FAIL: '$result2'\n";

// Test double quote with ENT_QUOTES (flag = 2) - should decode
$result3 = html_entity_decode('"Hello"', ENT_QUOTES);
echo ($result3 === '"Hello"') ? "QUOTES_DQ_OK\n" : "QUOTES_DQ_FAIL: '$result3'\n";

// Test mixed entities with ENT_NOQUOTES
$result4 = html_entity_decode('<div>"test"</div>', ENT_NOQUOTES);
echo ($result4 === '<div>"test"</div>') ? "NOQUOTES_MIXED_OK\n" : "NOQUOTES_MIXED_FAIL: '$result4'\n";

// Test that single quotes are still not decoded with ENT_NOQUOTES
$result5 = html_entity_decode("&#39;test&#39;", ENT_NOQUOTES);
echo ($result5 === "&#39;test&#39;") ? "NOQUOTES_SQ_OK\n" : "NOQUOTES_SQ_FAIL: '$result5'\n";
?>
--EXPECT--
NOQUOTES_DQ_OK
COMPAT_DQ_OK
QUOTES_DQ_OK
NOQUOTES_MIXED_OK
NOQUOTES_SQ_OK