--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SCOPE DIVERGENCE EUC-JP's third code set, JIS X 0212, has no table here and reads as an error character (PHL half)
--DESCRIPTION--
EUC-JP carries three code sets: ASCII, JIS X 0201's katakana behind 0x8E, and JIS X 0208
behind a 0xA1..0xFE pair. php carries a FOURTH, JIS X 0212 behind 0x8F, whose 6067 cells are a
second 94x94 plane with its own table. PHL ships the JIS X 0208 table only, so a 0x8F sequence
is an error character. Its LENGTH still matches php -- the whole three-byte sequence is one
character -- so everything after it in the string stays in step, and only the character itself
differs. SJIS has no 0x8F plane at all, so it is exact.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend member";
}
?>
--FILE--
<?php
mb_substitute_character(0x3F);
$s = "\x8f\xa2\xaf" . "AB";
var_dump(mb_strlen($s, 'EUC-JP'), mb_check_encoding($s, 'EUC-JP'));
echo bin2hex(mb_convert_encoding($s, 'UTF-8', 'EUC-JP')), "\n";
echo bin2hex(mb_scrub($s, 'EUC-JP')), "\n";
?>
--EXPECT--
int(3)
bool(false)
3f4142
3f4142
--CLEAN--
<?php
