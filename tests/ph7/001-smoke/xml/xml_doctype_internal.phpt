--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
xml doctype with internal subset
--FILE--
<?php
$parser = xml_parser_create();
xml_set_element_handler($parser, 'start_element', 'end_element');
xml_set_character_data_handler($parser, 'character_data');
$xml = '<!DOCTYPE root [ <!ELEMENT root (child)> <!ELEMENT child (#PCDATA)> ]><root><child>value</child></root>';
$result = xml_parse($parser, $xml, true);
$error = xml_get_error_code($parser);
$errstr = xml_error_string($error);
xml_parser_free($parser);

echo "Parse result: $result\n";
echo "Error code: $error\n";
echo "Error string: $errstr\n";

function start_element($parser, $name, $attrs) { echo "Start: $name\n"; }
function end_element($parser, $name) { echo "End: $name\n"; }
function character_data($parser, $data) { if(trim($data)) { echo "Data: $data\n"; } }
?>
--EXPECT--
Parse result: 0
Error code: 8
Error string: Tag mismatch
--CLEAN--
<?php
unset($parser, $xml, $result, $error, $errstr);
