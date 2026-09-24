--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_set_blocking really reaches the descriptor, and the chunk size really reaches the reads
--SKIPIF--
<?php
if (!function_exists('posix_mkfifo') && !is_executable('/usr/bin/mkfifo') && !is_executable('/bin/mkfifo')) {
    echo "skip needs mkfifo for a stream that would block";
}
if (DIRECTORY_SEPARATOR === '\\') {
    echo "skip POSIX fifo test";
}
?>
--FILE--
<?php
/* The whole point of stream_set_blocking(false) is that a read which WOULD
 * wait comes back empty instead. A fifo nobody has written to is the cheapest
 * stream that would wait; if the mode never reached the descriptor, this test
 * hangs rather than fails. */
$sse_fifo = sys_get_temp_dir() . '/phl_sse_' . getmypid() . '.fifo';
@unlink($sse_fifo);
exec('mkfifo ' . escapeshellarg($sse_fifo));

$h = fopen($sse_fifo, 'r+');
var_dump(stream_set_blocking($h, false));
var_dump(stream_get_meta_data($h)['blocked']);
/* Nothing has been written: php answers "" for a read that found nothing
 * rather than false, which means "the read failed". */
var_dump(fread($h, 16));
/* and nothing has ENDED either — a read that could not proceed is not an EOF */
var_dump(feof($h));
fwrite($h, 'now there is');
var_dump(fread($h, 16));
var_dump(stream_set_blocking($h, true));
var_dump(stream_get_meta_data($h)['blocked']);
fclose($h);
unlink($sse_fifo);

/* On a system where a plain file IS a descriptor, the mode reaches it and the
 * metadata says so — the key follows the descriptor, not a bookkeeping flag.
 * (A Windows file carries a HANDLE instead, which is why this lives in the
 * POSIX-gated half.) */
$b = fopen(__FILE__, 'r');
var_dump(socket_set_blocking($b, false));
var_dump(stream_get_meta_data($b)['blocked']);
var_dump(stream_set_blocking($b, true));
var_dump(stream_get_meta_data($b)['blocked']);
fclose($b);

/* The chunk size governs how much the line reader pulls off the device at a
 * time; it must not change what the reader ANSWERS. */
$sse_path = sys_get_temp_dir() . '/phl_sse_' . getmypid() . '.txt';
file_put_contents($sse_path, "one\ntwo\nthree\n");
$c = fopen($sse_path, 'r');
var_dump(stream_set_chunk_size($c, 1));
var_dump(fgets($c), fgets($c), fgets($c), fgets($c));
var_dump(feof($c));
fclose($c);
unlink($sse_path);
?>
--EXPECT--
bool(true)
bool(false)
string(0) ""
bool(false)
string(12) "now there is"
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
int(8192)
string(4) "one
"
string(4) "two
"
string(6) "three
"
bool(false)
bool(true)
