--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_* case mapping is Unicode's — every alphabet, the three mappings, and the title-case word boundary
--FILE--
<?php
// Beyond the five ranges a hand-written mapper covered: Latin Ext-B, Roman
// numerals, ligatures, Armenian, Georgian, Cherokee, Deseret, full-width.
echo mb_strtoupper("ǆdžⅷﬁßǰ"), "|", mb_strtolower("ǄDŽⅧİ"), "\n";
echo mb_strtoupper("ԁեჟꭰ"), "|", mb_strtoupper("ﬅ"), "|", mb_strtoupper("𐐨"), "|", mb_strtolower("𐐀"), "|", mb_strtoupper("ａｂ"), "\n";
// Turkish dotless/dotted i are irregular in both directions
echo mb_strtoupper("ı"), "|", bin2hex(mb_strtolower("İ")), "\n";

// TITLE is not UPPER: Unicode has a third form, and a mapping may expand
echo mb_convert_case("ǆx", MB_CASE_TITLE), "|", mb_convert_case("ßx", MB_CASE_TITLE), "|", mb_convert_case("ǰx", MB_CASE_TITLE), "\n";

// A word starts at a CASED character whose predecessor was neither cased nor
// case-ignorable: a digit and an uncased letter end a word, an apostrophe or a
// combining mark does not.
echo mb_convert_case("a1b o'neil a日b ab", MB_CASE_TITLE), "\n";

// Final_Sigma needs BOTH sides: a cased character before, none after
echo mb_strtolower("ΑΣ"), "|", mb_strtolower("Σ"), "|", mb_strtolower("ΣΣ"), "|", mb_strtolower("ΑΣ1"), "|", mb_strtolower("ΑΣΑ"), "\n";

// The case-insensitive search folds through the same tables: ς, σ and Σ are one
// character and the Kelvin sign is a k -- but ı is not an i
var_dump(mb_stripos("ΣΟΦΟΣ", "ς"), mb_stripos("KELVIN", "\u{212A}elvin"), mb_stripos("ı", "i"));

// mb_ucfirst title-cases the first character (so ß becomes Ss, not SS) and
// mb_lcfirst lowers it; an error character has no case and keeps its bytes
echo mb_ucfirst("ßx"), "|", mb_ucfirst("ǆx"), "|", mb_lcfirst("ΣΟΦΟΣ"), "|", bin2hex(mb_ucfirst("\xffabc")), "\n";
// and both honour $encoding, which the prelude pair used to drop
echo bin2hex(mb_ucfirst("\xe1x", "8bit")), "|", bin2hex(mb_ucfirst("\xe1x", "ASCII")), "|", bin2hex(mb_lcfirst("\xc1X", "8bit")), "\n";
?>
--EXPECT--
ǄDŽⅧFISSJ̌|ǆdžⅷi̇
ԀԵᲟᎠ|ST|𐐀|𐐨|ＡＢ
I|69cc87
ǅx|Ssx|J̌x
A1B O'neil A日B Ab
ας|σ|σς|ας1|ασα
int(0)
int(0)
bool(false)
Ssx|ǅx|σΟΦΟΣ|ff616263
c178|3f78|e158
--CLEAN--
<?php
