--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The HTML entity family's $encoding argument: ISO-8859-1 is one byte per character, in the table, the encoder and the decoder alike
--FILE--
<?php
// The $encoding argument was SCREENED and then dropped: every charset but UTF-8
// warned and was answered in UTF-8. `htmlentities($s, ENT_QUOTES, 'ISO-8859-1')`
// -- the ordinary call for a Latin-1 page -- therefore answered a table of 253
// rows where php answers 101, encoded a Latin-1 byte as a broken UTF-8 sequence,
// and decoded &eacute; to two bytes where php writes one.
$htx_L = 'ISO-8859-1';

// One byte per character, and its VALUE is the code point.
echo 'entities latin1:  ', bin2hex(htmlentities("\xe9\xa9&< abc", ENT_QUOTES, $htx_L)), "\n";
echo 'entities utf8:    ', bin2hex(htmlentities("\u{e9}\u{a9}&< abc", ENT_QUOTES, 'UTF-8')), "\n";
// A byte with no named entity passes through -- and it is a CHARACTER here, so
// nothing about it is ill-formed.
echo 'no entity for it: ', bin2hex(htmlentities("\x80\x81", ENT_QUOTES, $htx_L)), "\n";
echo 'specialchars:     ', bin2hex(htmlspecialchars("\xe9<&\xff", ENT_QUOTES, $htx_L)), "\n";
echo 'utf8 refuses it:  ', bin2hex(htmlspecialchars("\xff", ENT_QUOTES, 'UTF-8')), "\n";

// Decoding writes ONE byte, and leaves an entity the charset cannot hold alone.
echo 'decode latin1:    ', bin2hex(html_entity_decode('&eacute;&copy;&hearts;&#233;&#9829;&#xE9;', ENT_QUOTES, $htx_L)), "\n";
echo 'decode utf8:      ', bin2hex(html_entity_decode('&eacute;&copy;&hearts;&#233;&#9829;&#xE9;', ENT_QUOTES, 'UTF-8')), "\n";
echo 'decode specials:  ', bin2hex(html_entity_decode('&amp;&lt;&quot;&#039;&apos;', ENT_QUOTES, $htx_L)), "\n";
echo 'double_encode=0:  ', bin2hex(htmlentities("&eacute;\xe9", ENT_QUOTES, $htx_L, false)), "\n";

// The TABLE is the same rule: only the characters the charset has, keyed by the
// single byte that spells them.
foreach (['UTF-8', 'ISO-8859-1', 'ISO8859-1', 'iso-8859-1', ''] as $htx_cs) {
    $htx_t = get_html_translation_table(HTML_ENTITIES, ENT_QUOTES, $htx_cs);
    $htx_k = array_keys($htx_t);
    printf("table %-12s rows=%-4d first=%s last=%s eacute=%s\n", $htx_cs === '' ? '(default)' : $htx_cs,
        count($htx_t), bin2hex($htx_k[0]), bin2hex($htx_k[count($htx_k) - 1]), $htx_t["\xe9"] ?? $htx_t["\u{e9}"] ?? 'MISSING');
}
foreach ([ENT_QUOTES, ENT_NOQUOTES, ENT_COMPAT] as $htx_q) {
    echo 'latin1 quote flags ', $htx_q, ': ', count(get_html_translation_table(HTML_ENTITIES, $htx_q, $htx_L)),
        ' specialchars ', count(get_html_translation_table(HTML_SPECIALCHARS, $htx_q, $htx_L)), "\n";
}

// ENT_DISALLOWED replaces a character the doctype forbids with U+FFFD -- as a
// CHARACTER where the charset can hold one, and as the numeric REFERENCE for it
// where it cannot, which is every single-byte charset. The forbidden set covers
// the non-ASCII half too: 0x80-0x9F are control characters in Latin-1.
foreach ([$htx_L, 'UTF-8'] as $htx_cs) {
    foreach (["\x00", "\x0b", "\x7f", "\x81", "\x9f", "\xa0", "\xe9", 'A'] as $htx_b) {
        if ($htx_cs === 'UTF-8' && ord($htx_b) > 0x7f) { continue; }
        echo "disallowed $htx_cs ", bin2hex($htx_b), ': spec=', bin2hex(htmlspecialchars($htx_b, ENT_QUOTES | ENT_DISALLOWED, $htx_cs)),
            ' ent=', bin2hex(htmlentities($htx_b, ENT_QUOTES | ENT_DISALLOWED, $htx_cs)), "\n";
    }
}
echo 'disallowed mixed: ', bin2hex(htmlspecialchars("a\x0b\x81\xe9", ENT_QUOTES | ENT_DISALLOWED, $htx_L)), "\n";
echo 'disallowed xml1:  ', bin2hex(htmlspecialchars("\x0b\x81", ENT_QUOTES | ENT_DISALLOWED | ENT_XML1, $htx_L)), "\n";

// A charset php has and this engine does not keeps php's own warning.
$htx_err = [];
set_error_handler(function ($n, $m) use (&$htx_err) { $htx_err[] = $m; return true; });
foreach (['ASCII', 'US-ASCII', 'BOGUS', 'UTF8'] as $htx_cs) {
    echo "unsupported $htx_cs rows=", count(get_html_translation_table(HTML_ENTITIES, ENT_QUOTES, $htx_cs)), "\n";
}
restore_error_handler();
print_r($htx_err);
?>
--EXPECT--
entities latin1:  266561637574653b26636f70793b26616d703b266c743b20616263
entities utf8:    266561637574653b26636f70793b26616d703b266c743b20616263
no entity for it: 8081
specialchars:     e9266c743b26616d703bff
utf8 refuses it:  
decode latin1:    e9a9266865617274733be92623393832393be9
decode utf8:      c3a9c2a9e299a5c3a9e299a5c3a9
decode specials:  263c22272661706f733b
double_encode=0:  266561637574653b266561637574653b
table UTF-8        rows=253  first=22 last=e299a6 eacute=&eacute;
table ISO-8859-1   rows=101  first=22 last=ff eacute=&eacute;
table ISO8859-1    rows=101  first=22 last=ff eacute=&eacute;
table iso-8859-1   rows=101  first=22 last=ff eacute=&eacute;
table (default)    rows=253  first=22 last=e299a6 eacute=&eacute;
latin1 quote flags 3: 101 specialchars 5
latin1 quote flags 0: 99 specialchars 3
latin1 quote flags 2: 100 specialchars 4
disallowed ISO-8859-1 00: spec=262378464646443b ent=262378464646443b
disallowed ISO-8859-1 0b: spec=262378464646443b ent=262378464646443b
disallowed ISO-8859-1 7f: spec=262378464646443b ent=262378464646443b
disallowed ISO-8859-1 81: spec=262378464646443b ent=262378464646443b
disallowed ISO-8859-1 9f: spec=262378464646443b ent=262378464646443b
disallowed ISO-8859-1 a0: spec=a0 ent=266e6273703b
disallowed ISO-8859-1 e9: spec=e9 ent=266561637574653b
disallowed ISO-8859-1 41: spec=41 ent=41
disallowed UTF-8 00: spec=efbfbd ent=efbfbd
disallowed UTF-8 0b: spec=efbfbd ent=efbfbd
disallowed UTF-8 7f: spec=efbfbd ent=efbfbd
disallowed UTF-8 41: spec=41 ent=41
disallowed mixed: 61262378464646443b262378464646443be9
disallowed xml1:  262378464646443b81
unsupported ASCII rows=253
unsupported US-ASCII rows=253
unsupported BOGUS rows=253
unsupported UTF8 rows=253
Array
(
    [0] => get_html_translation_table(): Charset "ASCII" is not supported, assuming UTF-8
    [1] => get_html_translation_table(): Charset "US-ASCII" is not supported, assuming UTF-8
    [2] => get_html_translation_table(): Charset "BOGUS" is not supported, assuming UTF-8
    [3] => get_html_translation_table(): Charset "UTF8" is not supported, assuming UTF-8
)
