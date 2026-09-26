--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: the same diagnostic prints a stale errno (zend half)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[$no] ", preg_replace('/\(\d+\)/', '(<errno>)', $str), "\n"; return true; });
/* `=ZZ` is a `=` followed by one hex digit and a non-hex one, which php's
 * quoted-printable decoder refuses outright rather than passing through. The
 * number php prints is a stale errno — normalised here, because it is exactly
 * what is not reproducible. PHL prints 0; see the PHL half. */
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?="));
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?=", ICONV_MIME_DECODE_STRICT));
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?=", 0, "ISO-8859-1"));
/* CONTINUE_ON_ERROR hands the raw word over instead, and says nothing. */
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?=", ICONV_MIME_DECODE_CONTINUE_ON_ERROR));
restore_error_handler();
?>
--EXPECT--
[8] iconv_mime_decode(): Unknown error (<errno>)
bool(false)
[8] iconv_mime_decode(): Unknown error (<errno>)
bool(false)
[8] iconv_mime_decode(): Unknown error (<errno>)
bool(false)
string(15) "=?UTF-8?Q?=ZZ?="
--CLEAN--
<?php
