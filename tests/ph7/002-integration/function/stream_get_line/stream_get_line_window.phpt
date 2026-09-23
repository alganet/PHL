--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stream_get_line's length window bounds the ending search, php's own rule
--FILE--
<?php
// php's window rule: the ending counts only when it fits ENTIRELY inside the
// first $length bytes, and a capped read consumes no ending at its boundary.
$mkw = function (string $data) { $h = fopen("php://memory", "r+"); fwrite($h, $data); rewind($h); return $h; };
$h = $mkw("abcdefghij\nrest");
var_dump(stream_get_line($h, 4, "\n"));
var_dump(stream_get_line($h, 4, "\n"));
var_dump(stream_get_line($h, 4, "\n"));
var_dump(stream_get_line($h, 4, "\n"));
$h = $mkw("abcd\nrest");
var_dump(stream_get_line($h, 4, "\n"));
var_dump(stream_get_line($h, 4, "\n"));
var_dump(ftell($h));
$h = $mkw("abc--def");
var_dump(stream_get_line($h, 4, "--"));
var_dump(stream_get_line($h, 4, "--"));
$h = $mkw("abc-");
var_dump(stream_get_line($h, 100, "--"));
$h = $mkw("--");
var_dump(stream_get_line($h, 100, "--"));
var_dump(stream_get_line($h, 100, "--"));
// a line spanning several internal chunks
$h = $mkw(str_repeat("x", 20000) . "\nZZ");
var_dump(strlen(stream_get_line($h, 100000, "\n")));
var_dump(stream_get_line($h, 10, "\n"));
// validation
$h = $mkw("abc");
try { stream_get_line($h, -1, "\n"); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { stream_get_line("no", 10); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$c = fopen("php://memory", "r"); fclose($c);
try { stream_get_line($c, 10); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
string(4) "abcd"
string(4) "efgh"
string(2) "ij"
string(4) "rest"
string(4) "abcd"
string(0) ""
int(5)
string(4) "abc-"
string(4) "-def"
string(4) "abc-"
string(0) ""
bool(false)
int(20000)
string(2) "ZZ"
stream_get_line(): Argument #2 ($length) must be greater than or equal to 0
stream_get_line(): Argument #1 ($stream) must be of type resource, string given
stream_get_line(): Argument #1 ($stream) must be an open stream resource
