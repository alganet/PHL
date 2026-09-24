--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_get_meta_data() reports php's key set, order and labels
--FILE--
<?php
$sgmd_path = sys_get_temp_dir() . '/phl_sgmd_' . getmypid() . '.txt';
file_put_contents($sgmd_path, "hello\nworld\n");

/* Every handle is closed as soon as it has been described: Windows refuses to
 * unlink a file that still has one open. */
$sgmd_show = function ($label, $h) use ($sgmd_path) {
    $m = stream_get_meta_data($h);
    $out = [];
    foreach ($m as $k => $v) {
        if ($k === 'uri' && $v === $sgmd_path) { $v = '<TMP>'; }
        $out[] = $k . '=' . (is_bool($v) ? ($v ? 'true' : 'false') : (is_object($v) ? get_class($v) : $v));
    }
    echo $label, ': ', implode(' ', $out), "\n";
    fclose($h);
};

/* An ordinary file: php names the WRAPPER plainfile and the OPS STDIO, keeps
 * the mode as written and reports the path the opener was given. */
$sgmd_show('file r', fopen($sgmd_path, 'r'));
$sgmd_show('file rb', fopen($sgmd_path, 'rb'));

/* php://memory and php://temp are two labels over one idea. */
$sgmd_h = fopen('php://memory', 'r+');
fwrite($sgmd_h, 'abc');
rewind($sgmd_h);
$sgmd_show('memory', $sgmd_h);
/* The mode a memory stream reports is the one php BUILT, not the one it was
 * asked for: read-only stays "rb", an append open becomes "a+b". */
$sgmd_r = fopen('php://memory', 'r');
echo 'memory r mode: ', stream_get_meta_data($sgmd_r)['mode'], "\n";
fclose($sgmd_r);
$sgmd_a = fopen('php://memory', 'a');
echo 'memory a mode: ', stream_get_meta_data($sgmd_a)['mode'], "\n";
fclose($sgmd_a);
$sgmd_t = fopen('php://temp', 'w+');
fwrite($sgmd_t, 'abc');
rewind($sgmd_t);
$sgmd_show('temp', $sgmd_t);

/* data:// answers metadata of its OWN — the media type and whether the payload
 * was base64 — and php's three defaults are then NOT added. */
$sgmd_show('data b64', fopen('data://text/plain;base64,aGk=', 'r'));
$sgmd_show('data plain', fopen('data://text/html,hi', 'r'));
/* Every `;name=value` parameter is a key of its own, and a URI that names no
 * media type has NO mediatype key at all rather than the RFC's default. */
$sgmd_show('data params', fopen('data://text/plain;charset=utf-8;foo=bar;base64,aGk=', 'r'));
$sgmd_show('data untyped', fopen('data://,plain', 'r'));

/* eof is real: it was hardcoded FALSE, so a `while (!$m['eof'])` never ended. */
$sgmd_e = fopen($sgmd_path, 'r');
echo 'eof-at-start: ', var_export(stream_get_meta_data($sgmd_e)['eof'], true), "\n";
fread($sgmd_e, 100);
echo 'eof-after-read: ', var_export(stream_get_meta_data($sgmd_e)['eof'], true), "\n";
fclose($sgmd_e);

/* unread_bytes counts what the script's own reads left buffered. */
$sgmd_l = fopen($sgmd_path, 'r');
fgets($sgmd_l);
echo 'unread: ', stream_get_meta_data($sgmd_l)['unread_bytes'], "\n";
fclose($sgmd_l);

/* The keys that did not exist at all until now. */
$sgmd_k = fopen($sgmd_path, 'r');
$sgmd_m = stream_get_meta_data($sgmd_k);
echo 'uri-key: ', var_export(array_key_exists('uri', $sgmd_m), true), "\n";
echo 'mode-key: ', var_export(array_key_exists('mode', $sgmd_m), true), "\n";
fclose($sgmd_k);

/* php's own alias from the days sockets had a separate API. */
$sgmd_s = fopen($sgmd_path, 'r');
echo 'alias: ', socket_get_status($sgmd_s)['stream_type'], "\n";
fclose($sgmd_s);

unlink($sgmd_path);
?>
--EXPECT--
file r: timed_out=false blocked=true eof=false wrapper_type=plainfile stream_type=STDIO mode=r unread_bytes=0 seekable=true uri=<TMP>
file rb: timed_out=false blocked=true eof=false wrapper_type=plainfile stream_type=STDIO mode=rb unread_bytes=0 seekable=true uri=<TMP>
memory: timed_out=false blocked=true eof=false wrapper_type=PHP stream_type=MEMORY mode=w+b unread_bytes=0 seekable=true uri=php://memory
memory r mode: rb
memory a mode: a+b
temp: wrapper_type=PHP stream_type=TEMP mode=w+b unread_bytes=0 seekable=true uri=php://temp
data b64: mediatype=text/plain base64=true wrapper_type=RFC2397 stream_type=RFC2397 mode=r unread_bytes=0 seekable=true uri=data://text/plain;base64,aGk=
data plain: mediatype=text/html base64=false wrapper_type=RFC2397 stream_type=RFC2397 mode=r unread_bytes=0 seekable=true uri=data://text/html,hi
data params: mediatype=text/plain charset=utf-8 foo=bar base64=true wrapper_type=RFC2397 stream_type=RFC2397 mode=r unread_bytes=0 seekable=true uri=data://text/plain;charset=utf-8;foo=bar;base64,aGk=
data untyped: base64=false wrapper_type=RFC2397 stream_type=RFC2397 mode=r unread_bytes=0 seekable=true uri=data://,plain
eof-at-start: false
eof-after-read: true
unread: 6
uri-key: true
mode-key: true
alias: STDIO
