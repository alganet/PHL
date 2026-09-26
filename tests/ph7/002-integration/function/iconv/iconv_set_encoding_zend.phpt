--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: iconv_set_encoding() deprecates on every successful call (php half)
--DESCRIPTION--
The php half of iconv_set_encoding.phpt: each of the three $type values php
accepts writes an ini directive php 8.5 deprecates, so a call that WORKS always
emits one and a call that does not emits nothing. PHL removes the name outright
(§10) and keeps only the non-deprecated getter.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
var_dump(function_exists('iconv_set_encoding'));
// Every $type it accepts writes a directive php 8.5 deprecates, so every
// successful call emits one.
var_dump(iconv_set_encoding('internal_encoding', 'ISO-8859-1'));
var_dump(iconv_set_encoding('input_encoding', 'ISO-8859-1'));
var_dump(iconv_set_encoding('output_encoding', 'ISO-8859-1'));
// A $type it does not accept is a silent false — the only outcome with no
// deprecation attached, and it sets nothing.
var_dump(iconv_set_encoding('bogus', 'UTF-8'));
var_dump(iconv_get_encoding('internal_encoding'));
restore_error_handler();
?>
--EXPECT--
bool(true)
[8192] iconv_set_encoding(): Use of iconv.internal_encoding is deprecated
bool(true)
[8192] iconv_set_encoding(): Use of iconv.input_encoding is deprecated
bool(true)
[8192] iconv_set_encoding(): Use of iconv.output_encoding is deprecated
bool(true)
bool(false)
string(10) "ISO-8859-1"
--CLEAN--
<?php
