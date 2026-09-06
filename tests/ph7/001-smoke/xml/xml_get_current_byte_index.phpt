--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_get_current_byte_index() returns current byte position in parser
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 XML parser, not PHP's XML extension";
}
?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_element_handler($parser, 'start_element', 'end_element');

// Test that xml_get_current_byte_index returns a value
$byte_index = xml_get_current_byte_index($parser);
echo "Byte index: $byte_index\n";

// Parse some XML to move the parser
$xml = '<root><child>text</child></root>';
xml_parse($parser, $xml, true);

// Get byte index after parsing
$byte_index_after = xml_get_current_byte_index($parser);
echo "Byte index after parse: $byte_index_after\n";

xml_parser_free($parser);
?>
--EXPECTF--
Byte index: 0
Byte index after parse: 1
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
--CLEAN--
<?php
unset($parser, $byte_index, $xml, $byte_index_after);
