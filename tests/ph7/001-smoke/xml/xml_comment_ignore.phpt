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

--EXPECTF--
Start: root
Data: text
End: root
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
Parse result: 1
--CLEAN--
<?php
unset($parser, $xml, $result);
