--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stream_get_line consumes the ending, returns the remainder at EOF, false when empty
--FILE--
<?php
// The ending is consumed but never returned; EOF answers the remainder, and
// false only when nothing is left.
$mkbuf = function (string $data) { $h = fopen("php://memory", "r+"); fwrite($h, $data); rewind($h); return $h; };
$h = $mkbuf("alpha\nbeta\ngamma");
var_dump(stream_get_line($h, 1024, "\n"));
var_dump(stream_get_line($h, 1024, "\n"));
var_dump(stream_get_line($h, 1024, "\n"));
var_dump(stream_get_line($h, 1024, "\n"));
$h = $mkbuf("one--two--three");
var_dump(stream_get_line($h, 1024, "--"));
var_dump(stream_get_line($h, 1024, "--"));
var_dump(stream_get_line($h, 1024, "--"));
// consecutive endings produce empty strings, not skips
$h = $mkbuf("a\n\nb\n");
while (($l = stream_get_line($h, 1024, "\n")) !== false) { var_dump($l); }
// no ending argument: a plain length read
$h = $mkbuf("0123456789");
var_dump(stream_get_line($h, 4));
var_dump(stream_get_line($h, 4, ""));
var_dump(stream_get_line($h, 0));
?>
--EXPECT--
string(5) "alpha"
string(4) "beta"
string(5) "gamma"
bool(false)
string(3) "one"
string(3) "two"
string(5) "three"
string(1) "a"
string(0) ""
string(1) "b"
string(4) "0123"
string(4) "4567"
string(2) "89"
