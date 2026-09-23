--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fread() at end of file answers "", not false
--FILE--
<?php
/* php's fread() answers "" at EOF and false only for a stream it cannot read
 * from at all. PHL returned false for both, so the ordinary
 * `while (!feof($f)) $buf .= fread($f, 8192);` loop finished on the value that
 * means "the read failed", and a `=== false` guard fired at end of file. */
$fread_eof_name = tempnam(sys_get_temp_dir(), 'ph7_fread_eof_');

file_put_contents($fread_eof_name, '');
$fread_eof_fp = fopen($fread_eof_name, 'rb');
var_dump(fread($fread_eof_fp, 10), feof($fread_eof_fp));
var_dump(fread($fread_eof_fp, 10));      /* still EOF, still "" */
fclose($fread_eof_fp);

file_put_contents($fread_eof_name, 'ab');
$fread_eof_fp = fopen($fread_eof_name, 'rb');
var_dump(fread($fread_eof_fp, 2));   /* the whole file */
var_dump(fread($fread_eof_fp, 2));   /* nothing left */
var_dump(feof($fread_eof_fp));
/* (WHEN feof() flips is its own divergence -- PHL reads ahead to answer it, so
 * it is already true here while php's flag waits for a read that finds nothing.
 * Both agree once such a read has happened, which is what is asserted.) */
fclose($fread_eof_fp);

/* The whole-file idiom, which is what the wrong answer was reached through */
$fread_eof_fp = fopen($fread_eof_name, 'rb');
$fread_eof_buf = '';
while (!feof($fread_eof_fp)) {
    $fread_eof_chunk = fread($fread_eof_fp, 8192);
    if ($fread_eof_chunk === false) {
        echo "read failed\n";
        break;
    }
    $fread_eof_buf .= $fread_eof_chunk;
}
fclose($fread_eof_fp);
var_dump($fread_eof_buf);

/* php://memory has always answered this way; the two agree now */
$fread_eof_mem = fopen('php://memory', 'r+b');
fwrite($fread_eof_mem, 'z');
rewind($fread_eof_mem);
var_dump(fread($fread_eof_mem, 4), fread($fread_eof_mem, 4));
fclose($fread_eof_mem);
?>
--EXPECT--
string(0) ""
bool(true)
string(0) ""
string(2) "ab"
string(0) ""
bool(true)
string(2) "ab"
string(1) "z"
string(0) ""
--CLEAN--
<?php
@unlink($fread_eof_name);
unset($fread_eof_name, $fread_eof_fp, $fread_eof_buf, $fread_eof_chunk, $fread_eof_mem);
