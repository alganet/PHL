--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: xml_set_object() + method-name strings deprecate and work (php half)
--DESCRIPTION--
The php half of xml_set_object.phpt: php 8.4 deprecates both the function and
the non-callable-string handler spelling, then runs them anyway. PHL removes
the function and refuses the spelling (§10).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
$p = xml_parser_create();
class XmlSetObjectSink {
    public array $seen = [];
    public function onStart($parser, $name, $attrs): void { $this->seen[] = "S:$name"; }
    public function onEnd($parser, $name): void { $this->seen[] = "E:$name"; }
}
$sink = new XmlSetObjectSink;
var_dump(xml_set_object($p, $sink));
var_dump(xml_set_element_handler($p, "onStart", "onEnd"));
var_dump(xml_parse($p, "<a><b/></a>", true));
echo implode(",", $sink->seen), "\n";
?>
--EXPECT--
[8192] Function xml_set_object() is deprecated since 8.4, provide a proper method callable to xml_set_*_handler() functions
bool(true)
[8192] xml_set_element_handler(): Passing non-callable strings is deprecated since 8.4
bool(true)
int(1)
S:A,S:B,E:B,E:A
