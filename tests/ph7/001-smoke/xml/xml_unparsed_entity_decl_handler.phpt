--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_set_unparsed_entity_decl_handler() sets handler for unparsed entities
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

// Set unparsed entity declaration handler
xml_set_unparsed_entity_decl_handler($parser, 'unparsed_entity_handler');

// Test that handler can be set without error
echo "Handler set successfully\n";

function start_element($parser, $name, $attrs) {}
function end_element($parser, $name) {}
function unparsed_entity_handler($parser, $entity_name, $base, $system_id, $public_id, $notation_name) {
    echo "Unparsed entity: $entity_name\n";
}

xml_parser_free($parser);
?>
--EXPECTF--
Handler set successfully
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
--CLEAN--
<?php
unset($parser);
