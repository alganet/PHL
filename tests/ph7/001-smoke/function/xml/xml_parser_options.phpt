--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_parser_create()/create_ns() answer php 8's opaque XMLParser and the option surface reads and refuses exactly like php's
--FILE--
<?php
// The parser handle is php 8's opaque final class, made only by the factories.
$xp1 = xml_parser_create();
echo get_class($xp1), " ", (int)($xp1 instanceof XMLParser), "\n";
echo "ext: ", (int)extension_loaded('xml'), (int)extension_loaded('XML'), "\n";
// Option defaults: folding ON, everything else off/zero, target UTF-8.
var_dump(xml_parser_get_option($xp1, XML_OPTION_CASE_FOLDING));
var_dump(xml_parser_get_option($xp1, XML_OPTION_SKIP_TAGSTART));
var_dump(xml_parser_get_option($xp1, XML_OPTION_SKIP_WHITE));
var_dump(xml_parser_get_option($xp1, XML_OPTION_PARSE_HUGE));
var_dump(xml_parser_get_option($xp1, XML_OPTION_TARGET_ENCODING));
// Setters answer true; the value is read leniently ("yes" is truthy) and the
// target-encoding string is canonicalized to its uppercase spelling.
var_dump(xml_parser_set_option($xp1, XML_OPTION_CASE_FOLDING, "yes"));
var_dump(xml_parser_get_option($xp1, XML_OPTION_CASE_FOLDING));
var_dump(xml_parser_set_option($xp1, XML_OPTION_CASE_FOLDING, 0));
var_dump(xml_parser_get_option($xp1, XML_OPTION_CASE_FOLDING));
var_dump(xml_parser_set_option($xp1, XML_OPTION_TARGET_ENCODING, "iso-8859-1"));
var_dump(xml_parser_get_option($xp1, XML_OPTION_TARGET_ENCODING));
var_dump(xml_parser_set_option($xp1, XML_OPTION_SKIP_TAGSTART, 4));
var_dump(xml_parser_get_option($xp1, XML_OPTION_SKIP_TAGSTART));
// An unknown option is a ValueError on both ends; a negative tag-start is only
// a warning and answers false with the option unchanged.
foreach ([[9, 'set'], [0, 'get']] as [$opt, $mode]) {
    try {
        $mode === 'set' ? xml_parser_set_option($xp1, $opt, 1) : xml_parser_get_option($xp1, $opt);
    } catch (ValueError $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
set_error_handler(function ($no, $msg) { echo "W[$no] $msg\n"; return true; });
var_dump(xml_parser_set_option($xp1, XML_OPTION_SKIP_TAGSTART, -3));
restore_error_handler();
var_dump(xml_parser_get_option($xp1, XML_OPTION_SKIP_TAGSTART));
try {
    xml_parser_set_option($xp1, XML_OPTION_TARGET_ENCODING, "KOI8-R");
} catch (ValueError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The source-encoding screen takes three names case-insensitively and null/"".
foreach ([null, "", "utf-8", "ISO-8859-1", "US-ASCII"] as $enc) {
    echo get_class(xml_parser_create($enc));
}
echo "\n";
try {
    xml_parser_create("UTF-16");
} catch (ValueError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The class refuses new, clone and serialize -- the factories are the only door.
try { new XMLParser; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { clone $xp1; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { serialize($xp1); } catch (Exception $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// The XML_* constants: error codes 0..21, options 1..5, and the SAX vendor.
echo XML_ERROR_NONE, XML_ERROR_NO_MEMORY, XML_ERROR_SYNTAX, XML_ERROR_NO_ELEMENTS, "\n";
echo XML_ERROR_TAG_MISMATCH, " ", XML_ERROR_UNKNOWN_ENCODING, " ", XML_ERROR_EXTERNAL_ENTITY_HANDLING, "\n";
echo XML_OPTION_CASE_FOLDING, XML_OPTION_TARGET_ENCODING, XML_OPTION_SKIP_TAGSTART, XML_OPTION_SKIP_WHITE, XML_OPTION_PARSE_HUGE, "\n";
echo XML_SAX_IMPL, "\n";
// xml_error_string maps php's own expat-flavoured table, indexed by the RAW
// libxml code -- 76 is a mismatched tag out there, 3 an empty document.
echo xml_error_string(0), " / ", xml_error_string(3), " / ", xml_error_string(76), "\n";
echo xml_error_string(-1), " / ", xml_error_string(999), "\n";
?>
--EXPECT--
XMLParser 1
ext: 11
bool(true)
int(0)
bool(false)
bool(false)
string(5) "UTF-8"
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
string(10) "ISO-8859-1"
bool(true)
int(4)
ValueError: xml_parser_set_option(): Argument #2 ($option) must be a XML_OPTION_* constant
ValueError: xml_parser_get_option(): Argument #2 ($option) must be a XML_OPTION_* constant
W[2] xml_parser_set_option(): Argument #3 ($value) must be between 0 and 2147483647 for option XML_OPTION_SKIP_TAGSTART
bool(false)
int(4)
ValueError: xml_parser_set_option(): Argument #3 ($value) is not a supported target encoding
XMLParserXMLParserXMLParserXMLParserXMLParser
ValueError: xml_parser_create(): Argument #1 ($encoding) is not a supported source encoding
Error: Cannot directly construct XMLParser, use xml_parser_create() or xml_parser_create_ns() instead
Error: Trying to clone an uncloneable object of class XMLParser
Exception: Serialization of 'XMLParser' is not allowed
0123
7 18 21
12345
libxml
No error / Empty document / Mismatched tag
Unknown / Unknown
