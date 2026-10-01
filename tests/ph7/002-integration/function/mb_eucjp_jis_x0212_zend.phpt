--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SCOPE DIVERGENCE EUC-JP's third code set, JIS X 0212, is a character php decodes (php half)
--DESCRIPTION--
The php half of mb_eucjp_jis_x0212.phpt: php has the JIS X 0212 table and reads 0x8F 0xA2 0xAF
as U+02D8, the breve. PHL ships JIS X 0208 only and reads the same three bytes as one error
character -- the same LENGTH, so the rest of the string is unaffected.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
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
bool(true)
cb984142
8fa2af4142
--CLEAN--
<?php
