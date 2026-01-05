--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XML Parser Invalid CDATA Test
--DESCRIPTION--
Test that XML parser correctly handles incomplete CDATA sections.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_element_handler($parser, 'start_element', 'end_element');
xml_set_character_data_handler($parser, 'character_data');
$xml = '<root><![CDATA[ test </root>';
$result = xml_parse($parser, $xml, true);
$error_code = xml_get_error_code($parser);
$error_string = xml_error_string($error_code);
xml_parser_free($parser);

echo "Parse result: $result\n";
echo "Error code: $error_code\n";
echo "Error string: $error_string\n";

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
Parse result: 0
Error code: 6
Error string: Syntax error
