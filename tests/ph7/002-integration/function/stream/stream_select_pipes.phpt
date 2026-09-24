--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_select() over FILES and proc_open() pipes — the descriptors that are not sockets
--SKIPIF--
<?php
if (DIRECTORY_SEPARATOR === '\\') {
    /* RECORDED PLATFORM DIFFERENCE: a Windows file or pipe is a
     * HANDLE and select() takes only sockets, so PHL can represent none of them
     * — the call warns per stream and refuses. php emulates the wait with
     * WaitForMultipleObjects there. */
    echo "skip POSIX descriptors: Windows selects only sockets in PHL (§7.4)";
}
if (!function_exists('proc_open')) {
    echo "skip needs proc_open for a pipe that becomes readable";
}
?>
--FILE--
<?php
/* A stream_select() that only works on sockets is not the one php programs use:
 * the everyday caller is a process pump, waiting on the pipes of a child. */
$selp_f = fopen(__FILE__, 'r');
$r = [$selp_f]; $w = [$selp_f]; $x = null;
/* A regular file is always ready both ways — it never makes anyone wait. */
var_dump(stream_select($r, $w, $x, 0), count($r), count($w));

$selp_desc = [1 => ['pipe', 'w'], 2 => ['pipe', 'w']];
$selp_proc = proc_open('sleep 0.3; echo waited', $selp_desc, $selp_pipes);
var_dump(is_resource($selp_proc));

/* Nothing has been printed yet, so the wait EXPIRES: the pipe is not readable
 * and the array comes back empty. */
$r = [$selp_pipes[1]]; $w = null; $x = null;
var_dump(stream_select($r, $w, $x, 0, 50000), count($r));

/* And then it becomes readable, which is the whole point: this returns as soon
 * as the child prints rather than after the full timeout. */
/* Only the OUT pipe: whether the child has exited by now — closing stderr, and
 * a closed pipe reads as readable — is a race in either engine. */
$r = ['out' => $selp_pipes[1]]; $w = null; $x = null;
$selp_t = microtime(true);
var_dump(stream_select($r, $w, $x, 5), array_keys($r), microtime(true) - $selp_t < 4);
var_dump(rtrim(fgets($selp_pipes[1])));

/* The other end closing is READABLE too — that is how a pump learns the child
 * is done, and reading answers "" rather than blocking. */
$r = [$selp_pipes[1]]; $w = null; $x = null;
var_dump(stream_select($r, $w, $x, 5), count($r), fread($selp_pipes[1], 8), feof($selp_pipes[1]));
fclose($selp_pipes[1]);
fclose($selp_pipes[2]);
var_dump(proc_close($selp_proc));
fclose($selp_f);
?>
--EXPECT--
int(2)
int(1)
int(1)
bool(true)
int(0)
int(0)
int(1)
array(1) {
  [0]=>
  string(3) "out"
}
bool(true)
string(6) "waited"
int(1)
int(1)
string(0) ""
bool(true)
int(0)
--CLEAN--
<?php
unset($selp_f, $selp_desc, $selp_proc, $selp_pipes, $selp_t, $r, $w, $x);
