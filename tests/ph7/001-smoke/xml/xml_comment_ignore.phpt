--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml comment ignored by default
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
$xml = '<root><!-- a comment -->text</root>';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);

echo "Parse result: $result\n";
function start_element($parser, $name, $attrs) { echo "Start: $name\n"; }
function end_element($parser, $name) { echo "End: $name\n"; }
function character_data($parser, $data) { if(trim($data)) echo "Data: $data\n"; }
?>

--EXPECT--
Start: root
Data: text
End: root
Parse result: 1
--CLEAN--
<?php
unset($parser, $xml, $result);
