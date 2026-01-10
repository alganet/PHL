--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_set_default_handler can be set without error
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

// Set default handler for unhandled content
xml_set_default_handler($parser, 'default_handler');

$xml = '<root>Some text<![CDATA[cdata]]></root>';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);

echo "Parse result: $result\n";

function start_element($parser, $name, $attrs) { echo "Start: $name\n"; }
function end_element($parser, $name) { echo "End: $name\n"; }
function default_handler($parser, $data) {
    echo "Default: " . trim($data) . "\n";
}

?>
--EXPECT--
Start: root
End: root
Parse result: 1