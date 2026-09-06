--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_parser_set_object is deprecated but callable
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 XML parser, not PHP's XML extension";
}
?>
--FILE--
<?php
$parser = xml_parser_create();
// php's $object parameter is an object; the parser handle itself is not one here
xml_set_object($parser, new stdClass());
xml_set_element_handler($parser, 'startElement', 'endElement');

$elements = array();

function startElement($parser, $name, $attrs) {
    global $elements;
    $elements[] = $name;
}

function endElement($parser, $name) {
    // end element
}

$xml = '<root><child1/><child2/><child3/></root>';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);

echo "Parse result: $result\n";
echo "Elements found: " . count($elements) . "\n";
?>
--EXPECTF--
Error [3]: This function is depreceated and is a no-op.In order to mimic this behaviour,you can supply instead of a function name an array containing an object reference and a method name. in %s on line %d
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
Parse result: 1
Elements found: 4
--CLEAN--
<?php
unset($parser, $elements, $xml, $result);
