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
xml_set_object($parser, $parser);
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
--EXPECT--
Parse result: 1
Elements found: 4
--CLEAN--
<?php
unset($parser, $elements, $xml, $result);
