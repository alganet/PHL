--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_select() — waiting on sockets, the arrays it rewrites, and the buffer it answers from
--FILE--
<?php
class SelWrap {
    public $context;          /* php would create it dynamically otherwise */
    public $pos = 0;
    public $data = "alpha\nbeta\n";
    public function stream_open($path, $mode, $opt, &$opened) { return true; }
    public function stream_read($count) { $r = substr($this->data, $this->pos, $count); $this->pos += strlen($r); return $r; }
    public function stream_eof() { return $this->pos >= strlen($this->data); }
    public function stream_tell() { return $this->pos; }
    public function stream_seek($o, $w) { $this->pos = $o; return true; }
}
stream_wrapper_register('selwrap', 'SelWrap');
$sel_wrap = fopen('selwrap://x', 'r');
var_dump(fgets($sel_wrap), stream_get_meta_data($sel_wrap)['unread_bytes']);

/* The call that makes a php program WAIT on several streams at once. Without it
 * a non-blocking read can only say "not ready", never "ready now". */
$sel_srv = stream_socket_server('tcp://127.0.0.1:0');
$sel_nm = stream_socket_get_name($sel_srv, false);
$sel_port = (int)substr($sel_nm, strrpos($sel_nm, ':') + 1);

/* A LISTENING socket becomes readable when a connection is waiting: this is the
 * whole server loop. Nothing has connected yet, so the wait expires — with 0,
 * and with the arrays emptied. */
$r = [$sel_srv]; $w = null; $x = null;
$sel_t = microtime(true);
var_dump(stream_select($r, $w, $x, 0, 200000), $r, microtime(true) - $sel_t >= 0.1);

$sel_cli = stream_socket_client('tcp://127.0.0.1:' . $sel_port, $sel_e, $sel_es, 5);
$r = ['listener' => $sel_srv]; $w = null; $x = null;
/* Now one IS waiting, and the key the caller used comes back with it. */
var_dump(stream_select($r, $w, $x, 2), array_keys($r));
$sel_conn = stream_socket_accept($sel_srv, 2);

/* A connected socket with nothing on it is not readable, but IS writable —
 * and the two arrays are answered independently. */
$r = [$sel_conn]; $w = [$sel_conn]; $x = null;
var_dump(stream_select($r, $w, $x, 0), count($r), count($w));

/* The peer writes: the read set now names it. */
fwrite($sel_cli, 'abcdefgh');
$r = ['a' => $sel_conn, 'b' => $sel_cli]; $w = null; $x = null;
var_dump(stream_select($r, $w, $x, 2), array_keys($r));

/* php's buffered-data rule, and the reason a loop over a buffered handle does
 * not hang: a LINE read pulls a whole chunk off the device, so the bytes after
 * the first line sit in the handle's own buffer and the descriptor has nothing
 * left to report — select() would wait forever for data the script already has.
 * php answers the handle READY from its buffer instead, and empties the other
 * two sets while it does. */
var_dump(fread($sel_conn, 8));
fwrite($sel_cli, "line1\nline2\n");
usleep(100000);
var_dump(fgets($sel_conn), stream_get_meta_data($sel_conn)['unread_bytes']);
$r = [$sel_conn]; $w = [$sel_cli]; $x = null;
$sel_t = microtime(true);
var_dump(stream_select($r, $w, $x, 0, 0), count($r), count($w), microtime(true) - $sel_t < 0.1);
var_dump(fgets($sel_conn));

/* Two entries can name ONE descriptor: the arrays keep both, and the answer is
 * what the OS counted. */
$r = [$sel_conn, $sel_conn]; $w = null; $x = null;
fwrite($sel_cli, 'z');
var_dump(stream_select($r, $w, $x, 2), count($r));
var_dump(fread($sel_conn, 1));

/* The same shortcut is what lets a stream with NO descriptor at all take part:
 * a userland wrapper a line read has filled the buffer of is ready now, and
 * waiting on the OS for it would sleep out the whole timeout over bytes the
 * script is already holding. */
$r = ['wrap' => $sel_wrap, 'sock' => $sel_conn]; $w = null; $x = null;
$sel_t = microtime(true);
var_dump(@stream_select($r, $w, $x, 0, 300000), array_keys($r), microtime(true) - $sel_t < 0.1);
var_dump(fgets($sel_wrap));

/* And the two numbers are ONE timeout: php carries a $microseconds past a
 * million into the seconds rather than handing the OS a timeval it refuses. */
$r = [$sel_conn]; $w = null; $x = null;
$sel_t = microtime(true);
var_dump(stream_select($r, $w, $x, 0, 1000000), microtime(true) - $sel_t >= 0.9);

/* What it refuses. Nothing to wait on is an Error rather than an answer — for
 * three NULLs, for empty arrays, and for arrays holding nothing selectable. */
foreach ([0, 1, 2, 3, 4, 5] as $sel_case) {
    $r = [$sel_conn]; $w = null; $x = null;
    try {
        switch ($sel_case) {
            case 0: $r = null; stream_select($r, $w, $x, 0); break;
            case 1: $r = []; stream_select($r, $w, $x, 0); break;
            case 2: stream_select($r, $w, $x, -1); break;
            case 3: stream_select($r, $w, $x, 0, -1); break;
            /* A NULL $seconds is "wait forever", and there is no waiting
             * forever for five microseconds. */
            case 4: stream_select($r, $w, $x, null, 5); break;
            /* A bad entry is raised BEFORE the timeout is even looked at. */
            case 5: $r = [$sel_conn, 7]; stream_select($r, $w, $x, -1); break;
        }
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
/* A value that is not a stream, and a resource that is no longer open, are
 * worded apart — and only reach the caller when something else in the arrays
 * WAS selectable (otherwise the empty-arrays Error is what php raises). */
$r = [$sel_conn, 7]; $w = null; $x = null;
try { stream_select($r, $w, $x, 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$sel_dead = stream_socket_client('tcp://127.0.0.1:' . $sel_port, $sel_e, $sel_es, 5);
fclose($sel_dead);
$r = [$sel_conn, $sel_dead]; $w = null; $x = null;
try { stream_select($r, $w, $x, 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$r = [7]; $w = null; $x = null;
try { stream_select($r, $w, $x, 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* And a stream with no descriptor at all names its TYPE in a warning rather
 * than failing the call. */
$sel_mem = fopen('php://memory', 'r+');
$r = [$sel_conn, $sel_mem]; $w = null; $x = null;
fwrite($sel_cli, 'y');
var_dump(stream_select($r, $w, $x, 2), count($r));
fclose($sel_mem);
fclose($sel_conn);
fclose($sel_cli);
fclose($sel_srv);
?>
--EXPECTF--
string(6) "alpha
"
int(5)
int(0)
array(0) {
}
bool(true)
int(1)
array(1) {
  [0]=>
  string(8) "listener"
}
int(1)
int(0)
int(1)
int(1)
array(1) {
  [0]=>
  string(1) "a"
}
string(8) "abcdefgh"
string(6) "line1
"
int(6)
int(1)
int(1)
int(0)
bool(true)
string(6) "line2
"
int(1)
int(2)
string(1) "z"
int(1)
array(1) {
  [0]=>
  string(4) "wrap"
}
bool(true)
string(5) "beta
"
int(0)
bool(true)
ValueError: No stream arrays were passed
ValueError: No stream arrays were passed
ValueError: stream_select(): Argument #4 ($seconds) must be greater than or equal to 0
ValueError: stream_select(): Argument #5 ($microseconds) must be greater than or equal to 0
ValueError: stream_select(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null
TypeError: stream_select(): supplied argument is not a valid stream resource
TypeError: stream_select(): supplied argument is not a valid stream resource
TypeError: stream_select(): supplied resource is not a valid stream resource
ValueError: No stream arrays were passed
%AWarning:%Astream_select(): Cannot represent a stream of type MEMORY as a select()able descriptor%Aint(1)
int(1)
--CLEAN--
<?php
unset($sel_srv, $sel_nm, $sel_port, $sel_cli, $sel_conn, $sel_e, $sel_es, $sel_t,
      $sel_case, $sel_dead, $sel_mem, $sel_wrap, $r, $w, $x, $e);
