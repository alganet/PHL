--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fputcsv() answers the byte count it wrote, quotes every field php quotes, keeps empty fields, and honours $eol
--FILE--
<?php
function fpw_write(array $fpw_f, $fpw_sep = ',', $fpw_enc = '"', $fpw_esc = '\\', $fpw_eol = "\n") {
    $fpw_h = fopen('php://memory', 'r+');
    $fpw_n = fputcsv($fpw_h, $fpw_f, $fpw_sep, $fpw_enc, $fpw_esc, $fpw_eol);
    rewind($fpw_h);
    $fpw_s = stream_get_contents($fpw_h);
    fclose($fpw_h);
    // The count is the bytes that reached the stream, so it must equal what came back.
    return [$fpw_n, $fpw_n === strlen($fpw_s), bin2hex($fpw_s)];
}

// The return value is the byte count -- the documented `=== false` check needs
// one, and a caller totalling its output needs the number.
var_dump(fpw_write(['a', 'b']));

// An EMPTY field is still a field, so its delimiter is still written: dropping
// it shifts every later column by one.
var_dump(fpw_write(['', 'a']), fpw_write(['a', '']), fpw_write(['', '', '']), fpw_write([]));

// A field is enclosed when it holds the delimiter, the enclosure, the escape,
// or any of \n, \r, \t and SPACE. The newline is the one that matters most: an
// unquoted embedded newline reads back as two ROWS.
var_dump(fpw_write(["a\nb"]), fpw_write(["a\rb"]), fpw_write(["a\tb"]), fpw_write(['a b']));
var_dump(fpw_write(['a,b']), fpw_write(['a"b']), fpw_write(['a\\b']), fpw_write(['ab']));

// An embedded enclosure is DOUBLED -- unless the escape came right before it,
// in which case the pair passes through and the escape does not arm again.
var_dump(fpw_write(['"']), fpw_write(['""']), fpw_write(['a"b"c']), fpw_write(["a\\\"b"]));

// The three character arguments choose what all of that is measured against.
var_dump(fpw_write(['a;b', 'c'], ';'), fpw_write(["a'b", 'c'], ',', "'"));
var_dump(fpw_write(['a#b'], ',', '"', '#'), fpw_write(['a\\b'], ',', '"', ''));

// $eol is php 8.1's, takes ANY string including the empty one, and its default
// is "\n" on every platform -- not the host's line ending.
var_dump(fpw_write(['a'], ',', '"', '\\', "\r\n"));
var_dump(fpw_write(['a'], ',', '"', '\\', ''));
var_dump(fpw_write(['a'], ',', '"', '\\', 'END'));

// Non-string values take the ordinary string cast: true is "1", false and null
// are empty.
var_dump(fpw_write([1, 2.5, true, false, null]));

// The three character arguments are compared as BYTES, so a high one works.
var_dump(fpw_write(["a\xE9b", 'c'], "\xE9"), fpw_write(["a\xE9b\xE9c"], ',', "\xE9"));
var_dump(fpw_write(["a\xE9b"], ',', '"', "\xE9"), fpw_write(["a\xFFb"], "\xFF"));

// After a buffered read the line lands on the LOGICAL position, not the device
// one the line reader's read-ahead left behind.
$fpw_file = tempnam(sys_get_temp_dir(), 'fpw');
file_put_contents($fpw_file, "line1\nline2\nline3\n");
$fpw_fh = fopen($fpw_file, 'r+');
fgets($fpw_fh);
var_dump(fputcsv($fpw_fh, ['X', 'Y'], ',', '"', '\\'));
fclose($fpw_fh);
var_dump(file_get_contents($fpw_file));
unlink($fpw_file);

// A NUL is a byte like any other and does not end the field.
var_dump(fpw_write(["a\0b"]));

// The single-character arguments are still validated, and $escape alone may be
// the empty string.
try { fpw_write(['a'], ',,'); } catch (Throwable $fpw_e) { echo get_class($fpw_e), ': ', $fpw_e->getMessage(), "\n"; }
try { fpw_write(['a'], ',', ''); } catch (Throwable $fpw_e) { echo get_class($fpw_e), ': ', $fpw_e->getMessage(), "\n"; }
try { fpw_write(['a'], ',', '"', 'xx'); } catch (Throwable $fpw_e) { echo get_class($fpw_e), ': ', $fpw_e->getMessage(), "\n"; }

?>
--EXPECT--
array(3) {
  [0]=>
  int(4)
  [1]=>
  bool(true)
  [2]=>
  string(8) "612c620a"
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  bool(true)
  [2]=>
  string(6) "2c610a"
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  bool(true)
  [2]=>
  string(6) "612c0a"
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  bool(true)
  [2]=>
  string(6) "2c2c0a"
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  bool(true)
  [2]=>
  string(2) "0a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22610a62220a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22610d62220a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22610962220a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22612062220a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22612c62220a"
}
array(3) {
  [0]=>
  int(7)
  [1]=>
  bool(true)
  [2]=>
  string(14) "2261222262220a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22615c62220a"
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  bool(true)
  [2]=>
  string(6) "61620a"
}
array(3) {
  [0]=>
  int(5)
  [1]=>
  bool(true)
  [2]=>
  string(10) "222222220a"
}
array(3) {
  [0]=>
  int(7)
  [1]=>
  bool(true)
  [2]=>
  string(14) "2222222222220a"
}
array(3) {
  [0]=>
  int(10)
  [1]=>
  bool(true)
  [2]=>
  string(20) "2261222262222263220a"
}
array(3) {
  [0]=>
  int(7)
  [1]=>
  bool(true)
  [2]=>
  string(14) "22615c2262220a"
}
array(3) {
  [0]=>
  int(8)
  [1]=>
  bool(true)
  [2]=>
  string(16) "22613b62223b630a"
}
array(3) {
  [0]=>
  int(9)
  [1]=>
  bool(true)
  [2]=>
  string(18) "2761272762272c630a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "22612362220a"
}
array(3) {
  [0]=>
  int(4)
  [1]=>
  bool(true)
  [2]=>
  string(8) "615c620a"
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  bool(true)
  [2]=>
  string(6) "610d0a"
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  bool(true)
  [2]=>
  string(2) "61"
}
array(3) {
  [0]=>
  int(4)
  [1]=>
  bool(true)
  [2]=>
  string(8) "61454e44"
}
array(3) {
  [0]=>
  int(10)
  [1]=>
  bool(true)
  [2]=>
  string(20) "312c322e352c312c2c0a"
}
array(3) {
  [0]=>
  int(8)
  [1]=>
  bool(true)
  [2]=>
  string(16) "2261e96222e9630a"
}
array(3) {
  [0]=>
  int(10)
  [1]=>
  bool(true)
  [2]=>
  string(20) "e961e9e962e9e963e90a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "2261e962220a"
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  bool(true)
  [2]=>
  string(12) "2261ff62220a"
}
int(4)
string(18) "line1
X,Y
2
line3
"
array(3) {
  [0]=>
  int(4)
  [1]=>
  bool(true)
  [2]=>
  string(8) "6100620a"
}
ValueError: fputcsv(): Argument #3 ($separator) must be a single character
ValueError: fputcsv(): Argument #4 ($enclosure) must be a single character
ValueError: fputcsv(): Argument #5 ($escape) must be empty or a single character
