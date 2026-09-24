--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_get_meta_data() over a pipe, a userland wrapper and the std streams
--FILE--
<?php
function sgmdd_line($label, $h)
{
    $m = stream_get_meta_data($h);
    $out = [];
    foreach ($m as $k => $v) {
        $out[] = $k . '=' . (is_bool($v) ? ($v ? 'true' : 'false') : (is_object($v) ? get_class($v) : $v));
    }
    echo $label, ': ', implode(' ', $out), "\n";
}

/* A popen() pipe is opened by NO wrapper and has no path, so php reports the
 * mode and omits both `wrapper_type` and `uri`. The `eof` key is dropped here
 * because the two engines answer it from different models on a pipe nothing
 * has read yet — that divergence is pinned by the stream_eof_model twin. */
$p = popen('exit 0', 'r');
$pm = stream_get_meta_data($p);
unset($pm['eof']);
$out = [];
foreach ($pm as $k => $v) { $out[] = $k . '=' . (is_bool($v) ? ($v ? 'true' : 'false') : $v); }
echo 'pipe: ', implode(' ', $out), "\n";
pclose($p);

/* A userland wrapper is labelled `user-space` on both sides and hands the
 * serving INSTANCE back as `wrapper_data` — the only way a script can reach
 * the object behind an open stream. */
class SgmddWrapper
{
    public $context;
    public function stream_open($path, $mode, $options, &$opened_path) { return true; }
    public function stream_read($count) { return ''; }
    public function stream_eof() { return true; }
    public function stream_stat() { return []; }
}
stream_wrapper_register('sgmdd', 'SgmddWrapper');
sgmdd_line('userland', fopen('sgmdd://x', 'r'));

/* The exported std streams name themselves the way php's do. `seekable` is
 * left out: php answers it from what fd 1 actually POINTS AT (false down a
 * pipe, true into a file), so it is a property of the harness rather than of
 * the engine — PHL always answers true (§7.4). */
foreach (['STDOUT' => STDOUT, 'STDERR' => STDERR] as $label => $h) {
    $m = stream_get_meta_data($h);
    unset($m['seekable']);
    $out = [];
    foreach ($m as $k => $v) { $out[] = $k . '=' . (is_bool($v) ? ($v ? 'true' : 'false') : $v); }
    echo $label, ': ', implode(' ', $out), "\n";
}
?>
--EXPECT--
pipe: timed_out=false blocked=true stream_type=STDIO mode=r unread_bytes=0 seekable=false
userland: timed_out=false blocked=true eof=true wrapper_data=SgmddWrapper wrapper_type=user-space stream_type=user-space mode=r unread_bytes=0 seekable=true uri=sgmdd://x
STDOUT: timed_out=false blocked=true eof=false wrapper_type=PHP stream_type=STDIO mode=wb unread_bytes=0 uri=php://stdout
STDERR: timed_out=false blocked=true eof=false wrapper_type=PHP stream_type=STDIO mode=wb unread_bytes=0 uri=php://stderr
