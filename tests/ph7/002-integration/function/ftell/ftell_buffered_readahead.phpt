--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ftell and SEEK_CUR answer the logical position, not the device's read-ahead
--FILE--
<?php
// The line readers buffer ahead of the script's position: ftell() answers the
// LOGICAL position, and SEEK_CUR seeks relative to it, not to wherever the
// device's read-ahead stopped.
$h = fopen("php://memory", "r+"); fwrite($h, "abcdefghij\nrest-of-data"); rewind($h);
var_dump(fgets($h));
var_dump(ftell($h));
var_dump(fseek($h, 2, SEEK_CUR));
var_dump(fgets($h));
$h2 = fopen("php://memory", "r+"); fwrite($h2, "abc\ndefgh"); rewind($h2);
fgets($h2);
var_dump(ftell($h2));
var_dump(fread($h2, 3));
var_dump(ftell($h2));
$h3 = fopen("php://memory", "r+"); fwrite($h3, "alpha\nbeta\ngamma"); rewind($h3);
stream_get_line($h3, 1024, "\n");
var_dump(ftell($h3));
?>
--EXPECT--
string(11) "abcdefghij
"
int(11)
int(0)
string(10) "st-of-data"
int(4)
string(3) "def"
int(7)
int(6)
