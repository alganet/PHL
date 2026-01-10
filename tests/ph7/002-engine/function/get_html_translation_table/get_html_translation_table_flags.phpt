--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_html_translation_table with ENT_QUOTES flag
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test get_html_translation_table with ENT_QUOTES flag
$table = get_html_translation_table(ENT_QUOTES);
echo "ENT_QUOTES table contains single quote: ";
echo (isset($table["'"]) ? "true" : "false") . "\n";

echo "ENT_QUOTES table contains double quote: ";
echo (isset($table['"']) ? "true" : "false") . "\n";

echo "ENT_QUOTES table contains &: ";
echo (isset($table['&']) ? "true" : "false") . "\n";
?>
--EXPECT--
ENT_QUOTES table contains single quote: true
ENT_QUOTES table contains double quote: true
ENT_QUOTES table contains &: true