--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the stream_set_*/stream_supports_lock/stream_is_local settings family
--FILE--
<?php
$ss_path = sys_get_temp_dir() . '/phl_streamset_' . getmypid() . '.txt';
file_put_contents($ss_path, str_repeat("line\n", 10));

$ss_row = function ($label, $h, $blocks = true) {
    printf(
        "%-8s rbuf=%d wbuf=%d filebuf=%d chunk=%d chunk-again=%d lock=%s local=%s blocking=%s timeout=%s\n",
        $label,
        stream_set_read_buffer($h, 0),
        stream_set_write_buffer($h, 0),
        set_file_buffer($h, 8192),
        stream_set_chunk_size($h, 4096),
        stream_set_chunk_size($h, 8192),
        var_export(stream_supports_lock($h), true),
        var_export(stream_is_local($h), true),
        var_export(stream_set_blocking($h, false) === $blocks, true),
        var_export(stream_set_timeout($h, 1, 500), true)
    );
    fclose($h);
};

/* stream_set_chunk_size answers the PREVIOUS size, which is what makes the
 * setting restorable; the buffer pair is 0-accepted / -1-unsupported for every
 * stream; and stream_set_timeout is TRUE only where there is a transport to
 * wait on, so a file answers false rather than pretending its reads are
 * bounded. Whether the mode can be SET on a plain file is a platform question:
 * php sets it with O_NONBLOCK, which Windows does not have, so stream_set_blocking
 * answers false for a file there and `blocking` prints whether the answer was
 * the platform's. */
$ss_fd_mode = PHP_OS_FAMILY !== 'Windows';
$ss_row('file r', fopen($ss_path, 'r'), $ss_fd_mode);
$ss_row('memory', fopen('php://memory', 'r+'));
$ss_row('data', fopen('data://text/plain,hi', 'r'));

/* stream_is_local answers from the WRAPPER, not from the path. */
foreach (['/etc/hostname', 'relative.txt', 'php://memory', 'php://temp',
          'data://text/plain,hi', 'http://example.com/', 'nosuchwrapper://x', ''] as $ss_s) {
    printf("is_local(%-22s) = %s\n", var_export($ss_s, true), var_export(@stream_is_local($ss_s), true));
}

/* A stream that has no descriptor to set the mode ON keeps reporting itself
 * blocked, and one whose metadata carries no `blocked` key at all keeps not
 * carrying one — both true on every platform. */
$ss_m = fopen('php://memory', 'r+');
stream_set_blocking($ss_m, false);
var_dump(stream_get_meta_data($ss_m)['blocked']);
fclose($ss_m);
$ss_d = fopen('data://text/plain,hi', 'r');
stream_set_blocking($ss_d, false);
var_dump(array_key_exists('blocked', stream_get_meta_data($ss_d)));
fclose($ss_d);

/* socket_set_blocking is php's alias for the same routine. */
$ss_h = fopen($ss_path, 'r');
var_dump(socket_set_blocking($ss_h, false) === $ss_fd_mode);
fclose($ss_h);

/* A chunk size the stream could never use is refused where it is written. */
$ss_h = fopen($ss_path, 'r');
try { stream_set_chunk_size($ss_h, 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { stream_set_chunk_size($ss_h, -1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
fclose($ss_h);

/* And every member screens its handle the same way. */
try { stream_set_blocking('x', true); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { stream_supports_lock(42); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

unlink($ss_path);
?>
--EXPECT--
file r   rbuf=0 wbuf=-1 filebuf=-1 chunk=8192 chunk-again=4096 lock=true local=true blocking=true timeout=false
memory   rbuf=0 wbuf=-1 filebuf=-1 chunk=8192 chunk-again=4096 lock=false local=true blocking=true timeout=false
data     rbuf=0 wbuf=-1 filebuf=-1 chunk=8192 chunk-again=4096 lock=false local=false blocking=true timeout=false
is_local('/etc/hostname'       ) = true
is_local('relative.txt'        ) = true
is_local('php://memory'        ) = true
is_local('php://temp'          ) = true
is_local('data://text/plain,hi') = false
is_local('http://example.com/' ) = false
is_local('nosuchwrapper://x'   ) = true
is_local(''                    ) = true
bool(true)
bool(false)
bool(true)
ValueError: stream_set_chunk_size(): Argument #2 ($size) must be greater than 0
ValueError: stream_set_chunk_size(): Argument #2 ($size) must be greater than 0
TypeError: stream_set_blocking(): Argument #1 ($stream) must be of type resource, string given
TypeError: stream_supports_lock(): Argument #1 ($stream) must be of type resource, int given
