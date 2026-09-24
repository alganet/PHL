--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_internal_encoding and mb_substitute_character are SETTINGS, and mb_scrub reads them
--FILE--
<?php
// The internal encoding is remembered, canonicalised to php's four names, and
// is what every mb_ call takes when its own $encoding argument is absent.
echo mb_internal_encoding(), "|";
var_dump(mb_internal_encoding("utf8"));
echo mb_internal_encoding(), "|";
mb_internal_encoding("binary");
echo mb_internal_encoding(), "|";
mb_internal_encoding("latin1");
echo mb_internal_encoding(), "|";
mb_internal_encoding("US-ASCII");
echo mb_internal_encoding(), "\n";
mb_internal_encoding("8bit");
var_dump(mb_strlen("áb"), mb_ord("\xff"), bin2hex(mb_strtoupper("\xe1")), bin2hex(mb_substr("áb", 0, 1)));
mb_internal_encoding("UTF-8");
var_dump(mb_strlen("áb"), mb_ord("á"));

// The substitute character is a code point AND a mode, and php keeps them
// apart: asking for "long" does not forget the code point.
var_dump(mb_substitute_character());
echo bin2hex(mb_scrub("a\xffb")), "|", bin2hex(mb_scrub("abc")), "|", bin2hex(mb_scrub("\xe0\xa0")), "\n";
// a one-byte encoding has no error characters at all, so nothing is scrubbed
echo bin2hex(mb_scrub("a\xffb", "8bit")), "|", bin2hex(mb_scrub("a\xffb", "ASCII")), "\n";
mb_substitute_character(0x3042);
var_dump(mb_substitute_character());
echo bin2hex(mb_scrub("a\xffb")), "|", bin2hex(mb_substr("a\xffb", 1, 1)), "\n";
mb_substitute_character("none");
var_dump(mb_substitute_character());
echo "[", bin2hex(mb_scrub("a\xffb")), "][", bin2hex(mb_strtolower("A\xffB")), "]\n";
// "long" and "entity" spell out a code point the TARGET could not hold, which
// only a conversion knows; an error character falls back to the code point
mb_substitute_character(63);
mb_substitute_character("long");
echo bin2hex(mb_convert_encoding("á", "ASCII", "UTF-8")), "|", bin2hex(mb_scrub("a\xffb")), "|", bin2hex(mb_strtoupper("\xff", "8bit")), "\n";
mb_substitute_character("entity");
echo bin2hex(mb_convert_encoding("á", "ASCII", "UTF-8")), "\n";
mb_substitute_character(63);
echo bin2hex(mb_convert_encoding("á", "ASCII", "UTF-8")), "|", bin2hex(mb_strtoupper("\xff", "8bit")), "\n";
try { mb_substitute_character(1114112); } catch (ValueError $mbsu) { echo $mbsu->getMessage(), "\n"; }
try { mb_substitute_character("bogus"); } catch (ValueError $mbsu) { echo $mbsu->getMessage(), "\n"; }
// a numeric STRING is not a code point: php's union takes a string as one of
// its three names and only those
try { mb_substitute_character("63"); } catch (ValueError $mbsu) { echo $mbsu->getMessage(), "\n"; }
?>
--EXPECT--
UTF-8|bool(true)
UTF-8|8bit|ISO-8859-1|ASCII
int(3)
int(255)
string(2) "c1"
string(2) "c3"
int(2)
int(225)
int(63)
613f62|616263|3f
61ff62|613f62
int(12354)
61e3818262|e38182
string(4) "none"
[6162][6162]
552b4531|613f62|552b313738
26237845313b
3f|3f
mb_substitute_character(): Argument #1 ($substitute_character) is not a valid codepoint
mb_substitute_character(): Argument #1 ($substitute_character) must be "none", "long", "entity" or a valid codepoint
mb_substitute_character(): Argument #1 ($substitute_character) must be "none", "long", "entity" or a valid codepoint
--CLEAN--
<?php
/* both are VM-global settings and the smoke corpus shares one interpreter, so
 * hand the defaults back to whatever runs next */
mb_internal_encoding("UTF-8");
mb_substitute_character(63);
