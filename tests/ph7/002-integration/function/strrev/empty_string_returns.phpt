--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
string builtins answer "" for an empty subject, never null/false
--DESCRIPTION--
Four builtins carried the legacy "nothing to do -> bail out" shortcut and answered
null (strrev, nl2br, preg_quote — the last by never touching its result at all) or
false (base64_encode) for an empty subject, where php answers the empty STRING and
declares a `string` return. It matters because '' is falsy but !== false and !==
null: `if (base64_encode($s) === false)` and `strlen(strrev($s))` both change
behaviour on the empty input alone.
--FILE--
<?php
$fns = ['strrev', 'nl2br', 'preg_quote', 'base64_encode', 'strtolower', 'strtoupper',
        'ucfirst', 'lcfirst', 'ucwords', 'trim', 'ltrim', 'rtrim', 'quotemeta',
        'urlencode', 'rawurlencode', 'urldecode', 'rawurldecode', 'base64_decode',
        'addslashes', 'stripslashes', 'strip_tags', 'wordwrap', 'md5'];
foreach ($fns as $f) {
    if (!function_exists($f)) {
        printf("%-15s MISSING\n", $f);
        continue;
    }
    $r = $f('');
    printf("%-15s %s %s\n", $f, gettype($r), var_export($r, true));
}
// Non-empty subjects are untouched by the fix.
var_dump(strrev('abc'), nl2br("a\nb"), preg_quote('a.b'), base64_encode('abc'));
?>
--EXPECT--
strrev          string ''
nl2br           string ''
preg_quote      string ''
base64_encode   string ''
strtolower      string ''
strtoupper      string ''
ucfirst         string ''
lcfirst         string ''
ucwords         string ''
trim            string ''
ltrim           string ''
rtrim           string ''
quotemeta       string ''
urlencode       string ''
rawurlencode    string ''
urldecode       string ''
rawurldecode    string ''
base64_decode   string ''
addslashes      string ''
stripslashes    string ''
strip_tags      string ''
wordwrap        string ''
md5             string 'd41d8cd98f00b204e9800998ecf8427e'
string(3) "cba"
string(9) "a<br />
b"
string(4) "a\.b"
string(4) "YWJj"
--CLEAN--
<?php
