--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_copy_to_stream() over a pipe, and its seek-failure shape
--SKIPIF--
<?php
if (DIRECTORY_SEPARATOR === '\\') {
    echo "skip POSIX shell pipeline";
}
?>
--FILE--
<?php
$scp_path = sys_get_temp_dir() . '/phl_scp_' . getmypid() . '.txt';
file_put_contents($scp_path, str_repeat("abcde\n", 4));

/* A pipe has no position, but it can still be DRAINED — this is the shape that
 * used to need `fwrite($to, stream_get_contents($from))`, which reads the whole
 * source into memory first. */
$p = popen('cat ' . escapeshellarg($scp_path), 'r');
$d = fopen('php://memory', 'w+');
echo 'from pipe: ', stream_copy_to_stream($p, $d), "\n";
rewind($d);
echo 'same bytes: ', var_export(stream_get_contents($d) === file_get_contents($scp_path), true), "\n";
pclose($p);
fclose($d);

/* Asking one to SEEK is php's only failure short of a broken write, and it says
 * so twice — the stream cannot seek, and the seek did not happen. */
/* The child's stderr is muted: abandoning the pipe mid-write makes `cat`
 * report a broken pipe on one engine and not the other, which is the SHELL
 * talking, not the engine. */
$p = popen('cat ' . escapeshellarg($scp_path) . ' 2>/dev/null', 'r');
$d = fopen('php://memory', 'w+');
var_dump(@stream_copy_to_stream($p, $d, null, 3));
pclose($p);
fclose($d);

/* Straight into a file, which is the other everyday half. */
$out = sys_get_temp_dir() . '/phl_scp_out_' . getmypid() . '.txt';
$s = fopen($scp_path, 'r');
$d = fopen($out, 'w');
echo 'to file: ', stream_copy_to_stream($s, $d), "\n";
fclose($s);
fclose($d);
echo 'identical: ', var_export(file_get_contents($out) === file_get_contents($scp_path), true), "\n";

unlink($scp_path);
unlink($out);
?>
--EXPECT--
from pipe: 24
same bytes: true
bool(false)
to file: 24
identical: true
