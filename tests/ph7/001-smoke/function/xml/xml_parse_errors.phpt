--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error codes are raw libxml numbers, xml_error_string() maps php's own expat-flavoured table, and the external-entity screen stops the parse with expat's 21
--FILE--
<?php
// Error codes are RAW libxml numbers (76 = mismatched tag, 26 = undeclared
// entity, 5 = premature end), positions come from the parser's input state,
// and xml_error_string() maps the raw number through php's own table.
$xpe1 = xml_parser_create();
var_dump(xml_parse($xpe1, "<a>\n  <b>\n</a>", true));
var_dump(xml_get_error_code($xpe1));
var_dump(xml_error_string(xml_get_error_code($xpe1)));
var_dump(xml_get_current_line_number($xpe1));
var_dump(xml_get_current_column_number($xpe1));
var_dump(xml_get_current_byte_index($xpe1));
// A successful final parse: code 0, cursor at the end of the document.
$xpe2 = xml_parser_create();
var_dump(xml_parse($xpe2, "<a>x</a>", true));
var_dump(xml_get_error_code($xpe2), xml_get_current_line_number($xpe2), xml_get_current_column_number($xpe2), xml_get_current_byte_index($xpe2));
// Parsing anything after the final chunk answers 0. (What the error CODE
// says afterwards depends on the libxml2 build -- 0 on 2.9, 5 on newer --
// so only the return value is pinned here.)
var_dump(xml_parse($xpe2, "<b/>", true));
// An empty document ended is an error (the exact code moved between libxml2
// releases: 5 on 2.9, 4 on newer); an empty NON-final chunk is fine.
$xpe3 = xml_parser_create();
var_dump(xml_parse($xpe3, "", false));
var_dump(xml_parse($xpe3, "", true), xml_get_error_code($xpe3) !== XML_ERROR_NONE);
// An undefined entity: the default handler still sees the raw reference, then
// the parse fails with 26.
$xpe4 = xml_parser_create();
xml_set_character_data_handler($xpe4, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
xml_set_default_handler($xpe4, function ($p, $d) { echo "D"; var_export($d); echo "\n"; });
var_dump(xml_parse($xpe4, "<a>x&nope;y</a>", true), xml_get_error_code($xpe4));
// The byte index advances per event; line numbers count from 1.
$xpe5 = xml_parser_create();
xml_set_element_handler($xpe5, function ($p, $n, $a) {
    echo "S $n @", xml_get_current_byte_index($p), ",", xml_get_current_line_number($p), "\n";
}, null);
xml_parse($xpe5, "<a>\n<b/>\n<c/></a>", true);
// The notation/unparsed-entity declaration handlers, and the external-entity
// screen: a handler answering false stops the parse with expat's 21 -- a
// number whose libxml table entry is about something else entirely.
$xpe6 = xml_parser_create();
xml_set_notation_decl_handler($xpe6, function ($p, ...$a) { echo "NOT "; var_export($a); echo "\n"; });
xml_set_unparsed_entity_decl_handler($xpe6, function ($p, ...$a) { echo "UNP "; var_export($a); echo "\n"; });
xml_set_external_entity_ref_handler($xpe6, function ($p, ...$a) { echo "EXT "; var_export($a); echo "\n"; return true; });
$dtd = '<!DOCTYPE a [<!NOTATION gif SYSTEM "gif.exe"><!ENTITY pic SYSTEM "p.gif" NDATA gif><!ENTITY ext SYSTEM "e.xml">]><a>&ext;</a>';
var_dump(xml_parse($xpe6, $dtd, true), xml_get_error_code($xpe6));
$xpe7 = xml_parser_create();
xml_set_external_entity_ref_handler($xpe7, function ($p, ...$a) { return false; });
var_dump(xml_parse($xpe7, $dtd, true), xml_get_error_code($xpe7));
echo xml_error_string(21), "\n";
?>
--EXPECT--
int(0)
int(76)
string(14) "Mismatched tag"
int(3)
int(5)
int(14)
int(1)
int(0)
int(1)
int(9)
int(8)
int(0)
int(1)
int(0)
bool(true)
D'<a>'
C'x'
D'&nope;'
int(0)
int(26)
S A @2,1
S B @6,2
S C @11,3
NOT array (
  0 => 'gif',
  1 => false,
  2 => 'gif.exe',
  3 => false,
)
UNP array (
  0 => 'pic',
  1 => false,
  2 => 'p.gif',
  3 => false,
  4 => 'gif',
)
EXT array (
  0 => 'ext',
  1 => '',
  2 => 'e.xml',
  3 => false,
)
int(1)
int(0)
int(0)
int(21)
PEReference: forbidden within markup decl in internal subset
