--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: php://memory, php://temp and data:// streams (band D slice 1)
--FILE--
<?php
// php://memory round trip
$h = fopen("php://memory", "r+");
fwrite($h, "hello world");
rewind($h);
echo fread($h, 5), "|", ftell($h), "\n";
fseek($h, 6);
echo fread($h, 100), "\n";
fseek($h, 0, SEEK_END);
echo ftell($h), "\n";
fwrite($h, "!");
rewind($h);
echo fgets($h), "\n";
// overwrite mid-stream
fseek($h, 0);
fwrite($h, "HELLO");
rewind($h);
echo fread($h, 12), "\n";
fclose($h);
// php://temp
$t = fopen("php://temp", "w+");
fwrite($t, "abc");
rewind($t);
echo fread($t, 10), "\n";
fclose($t);
// data:// URIs
echo file_get_contents("data://text/plain,hello%20there"), "\n";
echo file_get_contents("data://text/plain;base64,SGVsbG8gQmFzZTY0"), "\n";
echo file_get_contents("data://,raw"), "\n";
$d = fopen("data://text/plain,streamed", "r");
echo fread($d, 6), "|", fread($d, 10), "\n";
fclose($d);
?>
--EXPECT--
hello|5
world
11
hello world!
HELLO world!
abc
hello there
Hello Base64
raw
stream|ed
--CLEAN--
<?php
unset($h, $t, $d);
