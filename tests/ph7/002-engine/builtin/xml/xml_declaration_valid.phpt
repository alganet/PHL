--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XML Declaration Valid Position Test
--DESCRIPTION--
Test that XML declarations are allowed as the first element in valid XML.
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
xml_set_character_data_handler($parser, 'character_data');
$xml = '<' . '?xml version="1.0" encoding="UTF-8"?' . '><root><child>test</child></root>';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);
echo "Parse result: $result\n";

function start_element($parser, $name, $attrs) {
    echo "Start: $name\n";
}

function end_element($parser, $name) {
    echo "End: $name\n";
}

function character_data($parser, $data) {
    if (trim($data)) {
        echo "Data: $data\n";
    }
}
?>
--EXPECT--
Start: root
Start: child
Data: test
End: child
End: root
Parse result: 1