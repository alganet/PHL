--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XML parser with empty input
--SKIPIF--
<?php
if (function_exists('zend_version')) echo "skip";
?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_element_handler($parser, 'start_element', 'end_element');
xml_set_character_data_handler($parser, 'character_data');
// Parse empty string
$result = xml_parse($parser, '', true);
xml_parser_free($parser);
echo "Parse result: " . (int)$result . "\n";

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
Parse result: 1
--CLEAN--
<?php
unset($parser, $result);
