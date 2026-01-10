--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_set_external_entity_ref_handler() sets handler for external entity references
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

// Set external entity reference handler
xml_set_external_entity_ref_handler($parser, 'external_entity_handler');

// Test that handler can be set without error
echo "External entity handler set successfully\n";

function start_element($parser, $name, $attrs) {}
function end_element($parser, $name) {}
function external_entity_handler($parser, $open_entity_name, $base, $system_id, $public_id) {
    echo "External entity: $open_entity_name\n";
    return true;
}

xml_parser_free($parser);
?>
--EXPECT--
External entity handler set successfully