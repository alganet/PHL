--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml duplicate attribute error
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
$xml = '<root attr="x" attr="y"></root>';
$result = xml_parse($parser, $xml, true);
$error = xml_get_error_code($parser);
$errstr = xml_error_string($error);
xml_parser_free($parser);

echo "Parse result: $result\n";
echo "Error code: $error\n";
echo "Error string: $errstr\n";

function start_element($parser, $name, $attrs) { echo "Start: $name\n"; }
function end_element($parser, $name) { echo "End: $name\n"; }
?>
--EXPECT--
Parse result: 0
Error code: 9
Error string: Duplicate attribute
