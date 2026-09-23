--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fwrite after a buffered line read writes at the logical position
--FILE--
<?php
// fwrite after a read that buffered ahead lands at the LOGICAL position: php
// overwrites what the line reader left unread, not the end of its read-ahead.
$h = fopen("php://memory", "r+"); fwrite($h, "abcdefghij\n0123456789"); rewind($h);
var_dump(fgets($h));
fwrite($h, "WW");
rewind($h);
var_dump(stream_get_contents($h));
$h3 = fopen("php://memory", "r+"); fwrite($h3, "aa\nbb\ncc\n"); rewind($h3);
fgets($h3);
fwrite($h3, "XX");
var_dump(fgets($h3));
rewind($h3);
var_dump(stream_get_contents($h3));
?>
--EXPECT--
string(11) "abcdefghij
"
string(21) "abcdefghij
WW23456789"
string(1) "
"
string(9) "aa
XX
cc
"
