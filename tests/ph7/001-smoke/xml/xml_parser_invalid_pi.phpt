--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XML Parser Invalid Processing Instruction Test
--DESCRIPTION--
Test that XML parser correctly handles incomplete processing instructions.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_element_handler($parser, 'start_element', 'end_element');
$xml = '<root><?xml version="1.0"</root>';
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
--EXPECT--
Parse result: 0
Error code: 6
Error string: Syntax error
--CLEAN--
<?php
unset($parser, $xml, $result, $error_code, $error_string);
