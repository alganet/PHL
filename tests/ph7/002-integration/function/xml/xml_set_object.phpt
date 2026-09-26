--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: xml_set_object() and the method-name-string handlers are refused (PHL half)
--DESCRIPTION--
php 8.4 DEPRECATES xml_set_object() and the non-callable-string handler
spelling it exists for ("provide a proper method callable to
xml_set_*_handler() functions"). PHL targets php's non-deprecated surface
(§10): xml_set_object() does not exist, and a string that does not resolve
as a CALLABLE is refused with php's callback TypeError -- an ordinary
callable string like "trim" still passes, and [$obj, 'method'] is the
supported spelling for methods. php's half is the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
var_dump(function_exists('xml_set_object'));
$p = xml_parser_create();
try {
    xml_set_object($p, new stdClass);
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
try {
    xml_set_element_handler($p, "startElement", "endElement");
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
try {
    xml_set_character_data_handler($p, "");
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The object-method spelling php KEEPS is the array callable, and it works.
class XmlSetObjectSink {
    public array $seen = [];
    public function onStart($parser, $name, $attrs): void { $this->seen[] = "S:$name"; }
    public function onEnd($parser, $name): void { $this->seen[] = "E:$name"; }
}
$sink = new XmlSetObjectSink;
var_dump(xml_set_element_handler($p, [$sink, 'onStart'], [$sink, 'onEnd']));
var_dump(xml_parse($p, "<a><b/></a>", true));
echo implode(",", $sink->seen), "\n";
?>
--EXPECT--
bool(false)
Error: Call to undefined function xml_set_object()
TypeError: xml_set_element_handler(): Argument #2 ($start_handler) must be a valid callback or null, function "startElement" not found or invalid function name
TypeError: xml_set_character_data_handler(): Argument #2 ($handler) must be a valid callback or null, function "" not found or invalid function name
bool(true)
int(1)
S:A,S:B,E:B,E:A
