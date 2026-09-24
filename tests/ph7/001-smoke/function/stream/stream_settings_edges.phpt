--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the stream settings family over a directory handle, an oversized chunk and file://
--FILE--
<?php
/* A DIRECTORY handle rides the same device as a file here and stores a DIR*
 * where a file stores its descriptor, so every setting that reaches for the
 * descriptor has to recognise it: php's dir ops carry a rewind (seekable) and
 * no lock. */
$sse_d = opendir(sys_get_temp_dir());
echo 'dir seekable: ', var_export(stream_get_meta_data($sse_d)['seekable'], true), "\n";
echo 'dir supports_lock: ', var_export(stream_supports_lock($sse_d), true), "\n";
closedir($sse_d);

/* php refuses a chunk size no int can hold rather than storing one the next
 * call would report back. */
$sse_f = fopen(__FILE__, 'r');
try { stream_set_chunk_size($sse_f, 3000000000); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo 'chunk unchanged: ', stream_set_chunk_size($sse_f, 100), "\n";
fclose($sse_f);

/* A read that FAILED is false on every handle, non-blocking or not — the empty
 * string is reserved for one that simply found nothing. */
$sse_w = sys_get_temp_dir() . '/phl_sse_wo_' . getmypid() . '.txt';
$sse_h = fopen($sse_w, 'w');
stream_set_blocking($sse_h, false);
var_dump(@fread($sse_h, 10));
fclose($sse_h);
unlink($sse_w);

/* `file://host/path` names a REMOTE host, which php refuses rather than reading
 * as a local path. */
echo 'file://x: ', var_export(stream_is_local('file://x'), true), "\n";
echo 'file:///abs: ', var_export(stream_is_local('file:///etc'), true), "\n";
?>
--EXPECT--
dir seekable: true
dir supports_lock: false
ValueError: stream_set_chunk_size(): Argument #2 ($size) is too large
chunk unchanged: 8192
bool(false)
file://x: false
file:///abs: true
