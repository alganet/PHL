--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fseek() with an unknown $whence answers -1 and leaves the cursor where it was
--FILE--
<?php
$fiw_f = fopen('php://memory', 'w+');
fwrite($fiw_f, "hello");
// the three real modes still work
rewind($fiw_f);
var_dump(fseek($fiw_f, 1, SEEK_SET), ftell($fiw_f));
var_dump(fseek($fiw_f, 2, SEEK_CUR), ftell($fiw_f));
var_dump(fseek($fiw_f, -1, SEEK_END), ftell($fiw_f));
// an unknown whence is refused: -1, and the cursor must NOT move. PHL used to pass
// the raw value to the driver, where anything that was not CUR/END behaved as
// SEEK_SET -- so this reported success AND silently rewound the stream.
foreach ([99, -1, 3, PHP_INT_MAX] as $fiw_w) {
    fseek($fiw_f, 2, SEEK_SET);
    var_dump(fseek($fiw_f, 0, $fiw_w), ftell($fiw_f));
}
// a whence that is not an int is COERCED and then judged, not silently treated as
// SEEK_SET (the guard used to be gated on is_int, so "99" slipped past it). Kept to
// STRING forms: a fractional float whence also trips php's own lossy-conversion
// deprecation, which the shared in-process runner surfaces as output.
foreach (["99", "3", "1"] as $fiw_w) {
    fseek($fiw_f, 2, SEEK_SET);
    var_dump(fseek($fiw_f, 0, $fiw_w), ftell($fiw_f));
}
// the stream is intact
rewind($fiw_f);
var_dump(stream_get_contents($fiw_f));
fclose($fiw_f);
?>
--EXPECT--
int(0)
int(1)
int(0)
int(3)
int(0)
int(4)
int(-1)
int(2)
int(-1)
int(2)
int(-1)
int(2)
int(-1)
int(2)
int(-1)
int(2)
int(-1)
int(2)
int(0)
int(2)
string(5) "hello"
--CLEAN--
<?php
