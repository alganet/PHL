--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_getcsv()/fgetcsv() keep empty fields, undouble the enclosure, leave whitespace alone, and read a quoted newline as ONE record
--FILE--
<?php
function csvr($csvr_s, $csvr_sep = ',', $csvr_enc = '"', $csvr_esc = '\\') {
    return json_encode(str_getcsv($csvr_s, $csvr_sep, $csvr_enc, $csvr_esc));
}

// An EMPTY field is a field, wherever it sits -- leading, interior, trailing.
echo csvr('a,b'), csvr(','), csvr('a,'), csvr(',a'), csvr('a,,b'), csvr('a,b,'), "\n";

// A record with no text at all is ONE NULL field, which is what tells a caller
// a blank line apart from a line holding one empty value.
echo csvr(''), csvr("\n"), csvr("\r\n"), csvr("\r"), csvr('""'), csvr(' '), "\n";

// Quoted fields: the enclosure comes off, a doubled one is one literal, and the
// content is handed back byte for byte -- the spaces it was quoted to keep
// included.
echo csvr('"a","b"'), csvr('"a,b",c'), csvr('"a""b"'), csvr('""""'), csvr('"a""""b"'), "\n";
echo csvr('" a "'), csvr('"  a  "'), csvr('" ", " "'), "\n";

// Whitespace in FRONT of an opening enclosure is not part of the field; without
// an enclosure behind it, it is.
echo csvr(' "a",b'), csvr('"", ""'), csvr(' a , b '), csvr("\ta\t,\tb\t"), "\n";

// Whatever trails a CLOSING enclosure, up to the delimiter, belongs to the
// field too -- php's own quirk, and programs read back what it produces.
echo csvr('"a"b'), csvr('"a"b"'), csvr('"a" ,b'), csvr('"a"  '), csvr('"x"y"z"'), "\n";

// The escape keeps BOTH bytes, and only inside an enclosure -- an escape in an
// unquoted field means nothing at all.
echo csvr('"a\\"b"'), csvr('"a\\\\",b'), csvr('a\\,b'), "\n";
echo csvr('"a\\"b"', ',', '"', ''), csvr('"a\\\\"', ',', '"', ''), "\n";

// The enclosure is tested BEFORE the escape, which only shows when they are the
// same character.
echo csvr('"aa"b|\'b', ',', '"', '"'), csvr('"a""b"', ',', '"', '"'), "\n";

// One trailing line ending comes off the record, and off an unquoted field's
// own text; everything else stays.
echo csvr("a,b\n"), csvr("a,b\r\n"), csvr("a\n\n"), csvr("\n\n"), csvr("a\r"), csvr("a\r\rb"), "\n";

// Inside an enclosure a newline is content, and if the text runs out with the
// enclosure still open the stripped ending goes back in.
echo csvr("\"a\nb\""), csvr("\"a\r\n\""), csvr("\"a\n"), csvr("a\nb"), "\n";

// A NUL is a byte like any other -- it does not end a field and is not trimmed.
echo csvr("x\0y"), csvr("\"x\0y\""), "\n";

// The three character arguments choose all of the above, high bytes included.
echo csvr("a;'b;c';d", ';', "'"), csvr("a\xE9b", "\xE9"), csvr("\xE9a\xE9", ',', "\xE9"), "\n";

// fgetcsv() reads a RECORD, not a line: a quoted newline continues onto the
// next one, and the record ends where the enclosure closes.
$csvr_f = tempnam(sys_get_temp_dir(), 'csvr');
file_put_contents($csvr_f, "a,\"b\nc\",d\ne,f\n\"x\r\ny\",z\r\n\"unterminated\n");
$csvr_h = fopen($csvr_f, 'r');
while (($csvr_r = fgetcsv($csvr_h, 0, ',', '"', '\\')) !== false) {
    echo json_encode($csvr_r), "\n";
}
fclose($csvr_h);

// $length caps the FIRST read of a record, not the continuation reads: a
// quoted value must not end on a chunk boundary.
foreach ([["\"a\nb\",c\nd\n", 3], ["\"a\r\nb\",c\r\n", 5], ["a,b\nc,d\n", 3]] as [$csvr_raw, $csvr_len]) {
    file_put_contents($csvr_f, $csvr_raw);
    $csvr_h = fopen($csvr_f, 'r');
    $csvr_o = [];
    while (($csvr_r = fgetcsv($csvr_h, $csvr_len, ',', '"', '\\')) !== false) { $csvr_o[] = $csvr_r; }
    fclose($csvr_h);
    echo json_encode($csvr_o), "\n";
}

// What fputcsv writes is what fgetcsv reads back, embedded newlines and all.
$csvr_rows = [['a', 'b'], ['', 'x'], ["a\nb", 'c'], ['a"b', 'd,e'], [' f ', "g\th"], ['', '']];
$csvr_h = fopen($csvr_f, 'w');
foreach ($csvr_rows as $csvr_row) { fputcsv($csvr_h, $csvr_row, ',', '"', '\\', "\n"); }
fclose($csvr_h);
$csvr_h = fopen($csvr_f, 'r');
$csvr_back = [];
while (($csvr_r = fgetcsv($csvr_h, 0, ',', '"', '\\')) !== false) { $csvr_back[] = $csvr_r; }
fclose($csvr_h);
unlink($csvr_f);
var_dump($csvr_back === $csvr_rows);
?>
--EXPECT--
["a","b"]["",""]["a",""]["","a"]["a","","b"]["a","b",""]
[null][null][null][null][""][" "]
["a","b"]["a,b","c"]["a\"b"]["\""]["a\"\"b"]
[" a "]["  a  "][" "," "]
["a","b"]["",""][" a "," b "]["\ta\t","\tb\t"]
["ab"]["ab\""]["a ","b"]["a  "]["xy\"z\""]
["a\\\"b"]["a\\\\","b"]["a\\","b"]
["a\\b\""]["a\\\\"]
["aab|'b"]["a\"b"]
["a","b"]["a","b"]["a"][""]["a"]["a\r\rb"]
["a\nb"]["a\r\n"]["a\n"]["a\nb"]
["x\u0000y"]["x\u0000y"]
["a","b;c","d"]["a","b"]["a"]
["a","b\nc","d"]
["e","f"]
["x\r\ny","z"]
["unterminated\n"]
[["a\nb","c"],["d"]]
[["a\r\nb","c"]]
[["a","b"],[null],["c","d"],[null]]
bool(true)
