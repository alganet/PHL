--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the namespace parser qualifies names as URI<sep>local with a ONE-byte separator, hides xmlns attributes and fires declaration events with false for the default prefix
--FILE--
<?php
// The namespace parser: names become URI<sep>local (the whole thing folded,
// URI included), xmlns attributes disappear from the attribute array, the
// declaration events fire BEFORE the element, and the default namespace's
// prefix is handed as bool false. The end-namespace event never fires --
// php's libxml layer does not emit it, so neither does this one.
$xpn1 = xml_parser_create_ns();
xml_set_start_namespace_decl_handler($xpn1, function ($p, $prefix, $uri) { echo "NS "; var_export($prefix); echo " "; var_export($uri); echo "\n"; });
xml_set_end_namespace_decl_handler($xpn1, function ($p, $prefix) { echo "NSEND "; var_export($prefix); echo "\n"; });
xml_set_element_handler($xpn1,
    function ($p, $n, $a) { echo "S $n "; var_export($a); echo "\n"; },
    function ($p, $n) { echo "E $n\n"; });
var_dump(xml_parse($xpn1, '<a xmlns="urn:d" xmlns:x="urn:u" b="1" x:c="2"><x:e f="3"/><plain/></a>', true));
// The separator is ONE byte: "" joins the parts directly, "##" separates
// with '#', "AB" with 'A'. Attribute values expand entities in NS mode too.
foreach (["", "##", "AB"] as $sep) {
    $xpn2 = xml_parser_create_ns("UTF-8", $sep);
    xml_set_element_handler($xpn2, function ($p, $n, $a) { echo "S "; var_export($n); echo " "; var_export($a); echo "\n"; }, null);
    var_dump(xml_parse($xpn2, '<!DOCTYPE r [<!ENTITY am "&#38;#38;">]><x:e xmlns:x="urn:u" v="a&amp;b"/>', true));
}
// Case folding OFF keeps every spelling, URI included.
$xpn3 = xml_parser_create_ns();
xml_parser_set_option($xpn3, XML_OPTION_CASE_FOLDING, 0);
xml_set_element_handler($xpn3, function ($p, $n, $a) { echo "S $n "; var_export($a); echo "\n"; }, null);
xml_parse($xpn3, '<Mixed xmlns="urn:Case" At="V"/>', true);
?>
--EXPECT--
NS false 'urn:d'
NS 'x' 'urn:u'
S URN:D:A array (
  'B' => '1',
  'URN:U:C' => '2',
)
S URN:U:E array (
  'F' => '3',
)
E URN:U:E
S URN:D:PLAIN array (
)
E URN:D:PLAIN
E URN:D:A
int(1)
S 'URN:UE' array (
  'V' => 'a&b',
)
int(1)
S 'URN:U#E' array (
  'V' => 'a&b',
)
int(1)
S 'URN:UAE' array (
  'V' => 'a&b',
)
int(1)
S urn:Case:Mixed array (
  'At' => 'V',
)
