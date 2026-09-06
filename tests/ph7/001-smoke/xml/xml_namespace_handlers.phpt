--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml start and end namespace handlers invoked
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 XML parser, not PHP's XML extension";
}
?>
--FILE--
<?php
$parser = xml_parser_create_ns();
xml_set_start_namespace_decl_handler($parser,'ns_start');
xml_set_end_namespace_decl_handler($parser,'ns_end');
xml_set_element_handler($parser, 'start_element', 'end_element');
xml_set_character_data_handler($parser, 'character_data');
$xml = '<root xmlns:ns="http://example.com"><ns:child>ok</ns:child></root>';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);

echo "Parse result: $result\n";
function ns_start($parser,$prefix,$uri){ echo "NS START: $prefix $uri\n"; }
function ns_end($parser,$prefix){ echo "NS END: $prefix\n"; }
function start_element($parser, $name, $attrs) { echo "Start: $name\n"; }
function end_element($parser, $name) { echo "End: $name\n"; }
function character_data($parser, $data) { if(trim($data)) { echo "Data: $data\n"; } }
?>
--EXPECTF--
NS START: ns http://example.com
Start: root
Start: http://example.com:child>
Data: ok
End: http://example.com:child>
End: root
NS END: ns
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
Parse result: 1
--CLEAN--
<?php
unset($parser, $xml, $result);
