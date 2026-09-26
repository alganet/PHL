--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a quoted-printable payload php cannot decode says "Unknown error (0)" (PHL half)
--DESCRIPTION--
php's _php_iconv_show_error() has no case for the status a refused
quoted-printable payload produces, so it falls to the default and prints the
process's `errno` — which nothing on that path has SET. The number is whatever
the last libc call left behind (22 and 84 come out of the same input in the
same run, depending on which conversion ran before it). PHL has no stale errno
to leak, so it says 0; everything else about the diagnostic — the level, the
wording, the FALSE — is php's. The _zend twin shows php's side.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[$no] $str\n"; return true; });
/* `=ZZ` is a `=` followed by one hex digit and a non-hex one, which php's
 * quoted-printable decoder refuses outright rather than passing through. */
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?="));
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?=", ICONV_MIME_DECODE_STRICT));
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?=", 0, "ISO-8859-1"));
/* CONTINUE_ON_ERROR hands the raw word over instead, and says nothing. */
var_dump(iconv_mime_decode("=?UTF-8?Q?=ZZ?=", ICONV_MIME_DECODE_CONTINUE_ON_ERROR));
restore_error_handler();
?>
--EXPECT--
[8] iconv_mime_decode(): Unknown error (0)
bool(false)
[8] iconv_mime_decode(): Unknown error (0)
bool(false)
[8] iconv_mime_decode(): Unknown error (0)
bool(false)
string(15) "=?UTF-8?Q?=ZZ?="
--CLEAN--
<?php
