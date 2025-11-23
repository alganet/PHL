--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XML Declaration In Middle Of Document Test
--DESCRIPTION--
Test that XML declarations are rejected when placed in the middle of XML documents.
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
$xml = '<root><child><' . '?xml version="1.0"?' . '></child></root>';
$result = xml_parse($parser, $xml, true);
$error_code = xml_get_error_code($parser);
$error_string = xml_error_string($error_code);
xml_parser_free($parser);
echo "Parse result: $result, error code: $error_code ($error_string)\n";

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
Parse result: 0, error code: 18 (Misplaced processing instruction)