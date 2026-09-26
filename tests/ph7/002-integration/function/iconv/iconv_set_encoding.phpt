--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: iconv_set_encoding() is removed (PHL half)
--DESCRIPTION--
iconv_set_encoding() has exactly three things it can set, and php 8.5
DEPRECATES all three: iconv.input_encoding, iconv.output_encoding and
iconv.internal_encoding each emit "Use of iconv.<name> is deprecated" on every
successful call. A function whose only successful outcome is a deprecation is
deprecated surface, so §10 removes it: the name does not exist here and the
call is a loud catchable Error. The GETTER is not deprecated and stays,
answering default_charset for all three (iconv_get_encoding.phpt). php's half
is the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
var_dump(function_exists('iconv_set_encoding'));
try {
    iconv_set_encoding('internal_encoding', 'ISO-8859-1');
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The three directives it would have written do not exist either.
var_dump(ini_get('iconv.internal_encoding'), ini_get('iconv.input_encoding'),
         ini_get('iconv.output_encoding'));
// So the one setting that moves this family is default_charset, and it moved
// nothing above.
var_dump(iconv_get_encoding('internal_encoding'));
ini_set('default_charset', 'ISO-8859-1');
var_dump(iconv_get_encoding('internal_encoding'));
?>
--EXPECT--
bool(false)
Error: Call to undefined function iconv_set_encoding()
bool(false)
bool(false)
bool(false)
string(5) "UTF-8"
string(10) "ISO-8859-1"
--CLEAN--
<?php
