--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the iconv string family's default $encoding follows default_charset
--SKIPIF--
<?php
// The rows are glibc's own tables and grammar, which PHL reproduces on every
// platform; a php over another iconv (libiconv on macOS and Windows) answers
// its library's, so only the oracle is skipped there.
if (function_exists('zend_version') && (!defined('ICONV_IMPL') || ICONV_IMPL !== 'glibc')) {
    echo 'skip the oracle iconv is not glibc';
}
?>
--FILE--
<?php
/* The `?string $encoding = null` the iconv string family shares is php's
 * INTERNAL encoding: `iconv.internal_encoding` falling back to
 * `default_charset`. The scope policy removes the three deprecated `iconv.*` directives, so
 * `default_charset` is the whole of it here -- and it is settable at runtime,
 * which is the only way to move this family's default. Process-isolated
 * because it writes an engine-wide setting. */
set_error_handler(function ($no, $str) { echo "  W: $str\n"; return true; });
var_dump(ini_get('default_charset'));
var_dump(iconv_strlen("a\xC3\xA9b"), iconv_strlen("a\xE9b"));
ini_set('default_charset', 'ISO-8859-1');
var_dump(iconv_strlen("a\xE9b"), iconv_strlen("a\xC3\xA9b"));
var_dump(bin2hex(iconv_substr("a\xE9b", 1, 1)));
var_dump(iconv_strpos("a\xE9b\xE9", "\xE9", 2));
ini_set('default_charset', 'ASCII');
var_dump(iconv_strlen("abc"), iconv_strlen("a\xE9b"));
ini_set('default_charset', 'UTF-8');
var_dump(iconv_strlen("a\xC3\xA9b"));
restore_error_handler();
?>
--EXPECT--
string(5) "UTF-8"
  W: iconv_strlen(): Detected an illegal character in input string
int(3)
bool(false)
int(3)
int(4)
string(2) "e9"
int(3)
  W: iconv_strlen(): Detected an illegal character in input string
int(3)
bool(false)
int(3)
--CLEAN--
<?php
