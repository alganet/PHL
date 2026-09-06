--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_set_notation_decl_handler() sets handler for notation declarations
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

// Set notation declaration handler
xml_set_notation_decl_handler($parser, 'notation_handler');

// Test that handler can be set without error
echo "Notation handler set successfully\n";

function start_element($parser, $name, $attrs) {}
function end_element($parser, $name) {}
function notation_handler($parser, $notation_name, $base, $system_id, $public_id) {
    echo "Notation: $notation_name\n";
}

xml_parser_free($parser);
?>
--EXPECTF--
Notation handler set successfully
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
--CLEAN--
<?php
unset($parser);
