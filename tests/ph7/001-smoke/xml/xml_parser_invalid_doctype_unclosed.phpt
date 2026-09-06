--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XML Parser Invalid DOCTYPE Unclosed Internal Subset Test
--DESCRIPTION--
Test that XML parser correctly handles DOCTYPE with unclosed internal subset.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_element_handler($parser, 'start_element', 'end_element');
$xml = '<!DOCTYPE root [';
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
?>
--EXPECTF--
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
Parse result: 0
Error code: 6
Error string: Syntax error
--CLEAN--
<?php
unset($parser, $xml, $result, $error_code, $error_string);
