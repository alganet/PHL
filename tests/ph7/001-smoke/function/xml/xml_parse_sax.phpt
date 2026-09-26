--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_parse() routes every SAX event the way php's compat layer does: folding, entity boundaries, default-handler fallbacks and the claimed-route swallowing rule
--FILE--
<?php
// The SAX routing rules, each one php's: folding uppercases element AND
// attribute names (ASCII only), character data splits at entity boundaries,
// a defined internal entity reaches the DEFAULT handler as raw "&name;",
// and events with no handler of their own fall through to the default one.
$xps1 = xml_parser_create();
xml_set_element_handler($xps1,
    function ($p, $name, $attrs) { echo "S $name "; var_export($attrs); echo "\n"; },
    function ($p, $name) { echo "E $name\n"; });
xml_set_character_data_handler($xps1, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
xml_set_processing_instruction_handler($xps1, function ($p, $t, $d) { echo "P $t "; var_export($d); echo "\n"; });
xml_set_default_handler($xps1, function ($p, $d) { echo "D"; var_export($d); echo "\n"; });
$doc = "<?xml version=\"1.0\"?>\n<!DOCTYPE a [<!ENTITY who \"world\">]>"
     . "<!-- note --><a xmlns:x=\"urn:u\" b=\"l1\nl2\" x:c=\"2\">t1&amp;t2&who;"
     . "<![CDATA[cd]]><Aé/>\n<b>z</b></a><?pi data?>";
var_dump(xml_parse($xps1, $doc, true));
var_dump(xml_get_error_code($xps1));
// The handlers receive the SAME parser object the parse was handed.
$xps2 = xml_parser_create();
xml_set_element_handler($xps2, function ($p, $n, $a) use (&$xps2) { var_dump($p === $xps2); }, null);
xml_parse($xps2, "<a/>", true);
// Default-handler-only routing: raw start tags with the ORIGINAL entity
// spellings, per-run character data, raw "&amp;" (no cdata route claimed),
// rebuilt comments/PIs, and the split open+close of a self-closing tag.
$xps3 = xml_parser_create();
xml_set_default_handler($xps3, function ($p, $d) { echo "D"; var_export($d); echo "\n"; });
var_dump(xml_parse($xps3, "<!DOCTYPE a [<!ENTITY e \"ev\">]><a b=\"q&e;r\">x&amp;y<em/><!-- c --><?p d?></a>", true));
// Claiming a route and then unsetting the handler SWALLOWS those events
// instead of handing them back to the default handler -- php's layering.
$xps4 = xml_parser_create();
xml_set_default_handler($xps4, function ($p, $d) { echo "D"; var_export($d); echo "\n"; });
xml_set_character_data_handler($xps4, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
xml_set_character_data_handler($xps4, null);
var_dump(xml_parse($xps4, "<a>text</a>", true));
// A chunked document: libxml's push buffering merges runs split mid-text.
$xps5 = xml_parser_create();
xml_set_element_handler($xps5, function ($p, $n, $a) { echo "S $n\n"; }, function ($p, $n) { echo "E $n\n"; });
xml_set_character_data_handler($xps5, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
var_dump(xml_parse($xps5, "<a>he"));
var_dump(xml_parse($xps5, "llo<"));
var_dump(xml_parse($xps5, "/a>"));
var_dump(xml_parse($xps5, "", true));
// SKIP_TAGSTART strips the front of every element name it presents;
// SKIP_WHITE does nothing to SAX character data (it is an into_struct rule).
$xps6 = xml_parser_create();
xml_parser_set_option($xps6, XML_OPTION_SKIP_TAGSTART, 2);
xml_parser_set_option($xps6, XML_OPTION_SKIP_WHITE, 1);
xml_set_element_handler($xps6, function ($p, $n, $a) { echo "S "; var_export($n); echo "\n"; }, function ($p, $n) { echo "E "; var_export($n); echo "\n"; });
xml_set_character_data_handler($xps6, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
xml_parse($xps6, "<ns:a> <x/></ns:a>", true);
// A handler that throws: the exception surfaces from xml_parse itself, the
// remaining events are dropped, and the parser answers 0 afterwards.
$xps7 = xml_parser_create();
xml_set_element_handler($xps7, function ($p, $n, $a) { if ($n === "B") { throw new RuntimeException("boom"); } echo "S $n\n"; }, function ($p, $n) { echo "E $n\n"; });
try {
    xml_parse($xps7, "<a><b/><c/></a>", true);
} catch (RuntimeException $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
var_dump(xml_get_error_code($xps7));
// Calling back into the SAME parser from a handler is refused loudly.
$xps8 = xml_parser_create();
xml_set_element_handler($xps8, function ($p, $n, $a) {
    try { xml_parse($p, "<x/>", true); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}, null);
var_dump(xml_parse($xps8, "<a/>", true));
?>
--EXPECT--
D'<!-- note -->'
S A array (
  'XMLNS:X' => 'urn:u',
  'B' => 'l1 l2',
  'X:C' => '2',
)
C't1'
C'&'
C't2'
D'&who;'
C'cd'
S Aé array (
)
E Aé
C'
'
S B array (
)
C'z'
E B
E A
P pi 'data'
int(1)
int(0)
bool(true)
D'<a b="q&e;r">'
D'x'
D'&amp;'
D'y'
D'<em>'
D'</em>'
D'<!-- c -->'
D'<?p d?>'
D'</a>'
int(1)
D'<a>'
D'</a>'
int(1)
S A
int(1)
C'hello'
int(1)
E A
int(1)
int(1)
S ':A'
C' '
S ''
E ''
E ':A'
S A
caught: boom
int(0)
Error: Parser must not be called recursively
int(1)
