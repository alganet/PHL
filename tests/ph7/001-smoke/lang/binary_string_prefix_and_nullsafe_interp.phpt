--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The b/B binary-string prefix is part of the string, and "$o?->p" interpolates a nullsafe property fetch
--FILE--
<?php
// php's binary-string prefix marks nothing (php has one string type) and is part
// of the STRING, not an identifier in front of it.
$blx_v = 'V';
var_dump(b'foo', B'foo', b"foo", B"foo", b"x$blx_v", b'');
var_dump(b'a' === 'a', b"a" === "a", strlen(b"ab"));
$blx_h = b<<<EOT
hi $blx_v
EOT;
$blx_n = b<<<'EOT'
raw $blx_v
EOT;
var_dump($blx_h, $blx_n);
// Adjacency is the whole rule: `bb'x'` is an identifier then a string, and
// `b <<<` is too -- so this stays a concatenation of a constant and a string.
define('bb', 'CONST');
var_dump(bb . 'x');
// The nullsafe arrow in the simple interpolation syntax, on the same terms as
// '->': one property name, and only when a label follows.
$blx_o = new stdClass;
$blx_o->p = 'P';
$blx_o->q = null;
var_dump("$blx_o?->p", "$blx_o?->p tail", "{$blx_o?->p}");
$blx_null = null;
var_dump("[$blx_null?->p]");
var_dump(<<<EOT
$blx_o?->p
EOT);
// Not a label after the arrow: the bytes stay literal, exactly as with '->'.
var_dump("$blx_v?->", "$blx_v?-", "$blx_v?->1");
?>
--EXPECT--
string(3) "foo"
string(3) "foo"
string(3) "foo"
string(3) "foo"
string(2) "xV"
string(0) ""
bool(true)
bool(true)
int(2)
string(4) "hi V"
string(10) "raw $blx_v"
string(6) "CONSTx"
string(1) "P"
string(6) "P tail"
string(1) "P"
string(2) "[]"
string(1) "P"
string(4) "V?->"
string(3) "V?-"
string(5) "V?->1"
