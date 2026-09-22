--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_quote(): every byte, and NUL spelled "\000" like php
--DESCRIPTION--
A NUL in the subject came out as a backslash followed by a raw NUL byte, which is
not an escape at all — pcre reads that backslash as quoting whatever FOLLOWS the
NUL, so the quoted pattern stops meaning what the subject said. php writes the
three-digit escape "\000". The rest of the byte table (which characters take a
backslash, and the $delimiter argument) was already php-exact; this pins all 256
so it stays that way.
--FILE--
<?php
$all = '';
for ($i = 0; $i < 256; $i++) {
    $all .= chr($i);
}
echo bin2hex(preg_quote($all)), "\n";
echo bin2hex(preg_quote("a\x00b")), "\n";
var_dump(preg_quote("a\x00b") === 'a\\000b');
echo preg_quote('a/b#c', '/'), "\n";
echo preg_quote('a#b', '#'), "\n";
var_dump(preg_quote(''));

// A quoted subject matches itself, NUL included.
$s = "a\x00b.c";
var_dump(preg_match('/^' . preg_quote($s, '/') . '$/', $s));
?>
--EXPECT--
5c3030300102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f205c21225c235c242526275c285c295c2a5c2b2c5c2d5c2e2f303132333435363738395c3a3b5c3c5c3d5c3e5c3f404142434445464748494a4b4c4d4e4f505152535455565758595a5c5b5c5c5c5d5c5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a5c7b5c7c5c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
615c30303062
bool(true)
a\/b\#c
a\#b
string(0) ""
int(1)
--CLEAN--
<?php
