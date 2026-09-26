--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the target encoding converts names, values and character data on the way out (Latin-1 bytes, '?' for the rest), leaving the parser itself in UTF-8
--FILE--
<?php
// Target encoding conversion happens on the way OUT to the handlers:
// ISO-8859-1 keeps Latin-1 code points as bytes and turns the rest into '?',
// US-ASCII cuts at 0x7F -- names and values and character data alike. The
// data STAYS UTF-8 inside the parser, so the same document re-parses to
// different bytes per target.
foreach (["UTF-8", "ISO-8859-1", "US-ASCII"] as $target) {
    $xpc = xml_parser_create();
    xml_parser_set_option($xpc, XML_OPTION_TARGET_ENCODING, $target);
    xml_set_element_handler($xpc, function ($p, $n, $a) { echo "S "; var_export($n); echo " "; var_export($a); echo "\n"; }, null);
    xml_set_character_data_handler($xpc, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
    var_dump(xml_parse($xpc, "<aé at=\"café\">h\xc3\xa9z €</aé>", true));
}
// create()'s $encoding names the TARGET too (the input side auto-detects).
$xpc2 = xml_parser_create("ISO-8859-1");
var_dump(xml_parser_get_option($xpc2, XML_OPTION_TARGET_ENCODING));
// An invalid UTF-8 byte in character data becomes one '?' per byte.
$xpc3 = xml_parser_create();
xml_parser_set_option($xpc3, XML_OPTION_TARGET_ENCODING, "ISO-8859-1");
xml_set_character_data_handler($xpc3, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
var_dump(xml_parse($xpc3, "<a>ok</a>", true));
// Attribute values are normalized (tab/newline to space) and entity-expanded
// before any handler sees them, in every target encoding.
$xpc4 = xml_parser_create();
xml_set_element_handler($xpc4, function ($p, $n, $a) { var_export($a); echo "\n"; }, null);
var_dump(xml_parse($xpc4, "<!DOCTYPE a [<!ENTITY e \"ev\">]><a b=\"l1\nl2\tl3\" c=\"q&e;r&amp;s\"/>", true));
?>
--EXPECT--
S 'Aé' array (
  'AT' => 'café',
)
C'h'
C'éz €'
int(1)
S 'A�' array (
  'AT' => 'caf�',
)
C'h'
C'�z ?'
int(1)
S 'A?' array (
  'AT' => 'caf?',
)
C'h'
C'?z ?'
int(1)
string(10) "ISO-8859-1"
C'ok'
int(1)
array (
  'B' => 'l1 l2 l3',
  'C' => 'qevr&s',
)
int(1)
