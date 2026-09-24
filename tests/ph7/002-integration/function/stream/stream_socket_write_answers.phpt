--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what a socket WRITE answers — the byte count, a partial write, and a peer that has gone
--FILE--
<?php
/* fwrite() answers the number of bytes it MOVED. The socket device used to
 * hand back its own success STATUS — 0 — so every write that worked reported
 * nothing written: `fwrite($s, $buf) === strlen($buf)` failed on a successful
 * write and a partial-write retry loop never advanced. */
$ssw_srv = stream_socket_server('tcp://127.0.0.1:0');
$ssw_nm = stream_socket_get_name($ssw_srv, false);
$ssw_port = (int)substr($ssw_nm, strrpos($ssw_nm, ':') + 1);
$ssw_cli = stream_socket_client('tcp://127.0.0.1:' . $ssw_port, $ssw_e, $ssw_es, 5);
$ssw_conn = stream_socket_accept($ssw_srv, 5);

var_dump(fwrite($ssw_cli, 'hello'));
var_dump(fwrite($ssw_cli, 'hello', 3));
/* A socket read answers what has ARRIVED, and the second write may still be in
 * flight (macOS's loopback), so read until all eight are in. */
$ssw_got = '';
while (strlen($ssw_got) < 8 && !feof($ssw_conn)) {
    $ssw_got .= fread($ssw_conn, 8 - strlen($ssw_got));
}
var_dump($ssw_got);

/* A non-blocking write that cannot take everything answers what it took, and
 * one that can take nothing answers 0 — never false, which means "this handle
 * cannot be written to at all". */
stream_set_blocking($ssw_cli, false);
$ssw_chunk = str_repeat('x', 256 * 1024);
$ssw_counts = [];
for ($ssw_i = 0; $ssw_i < 200; $ssw_i++) {
    $ssw_w = fwrite($ssw_cli, $ssw_chunk);
    $ssw_counts[] = $ssw_w;
    if ($ssw_w === false || $ssw_w === 0) {
        break;
    }
}
$ssw_last = array_pop($ssw_counts);
var_dump($ssw_last);
var_dump(count($ssw_counts) > 0, array_sum($ssw_counts) > 0);
$ssw_shape = true;
foreach ($ssw_counts as $ssw_c) {
    if (!is_int($ssw_c) || $ssw_c <= 0 || $ssw_c > strlen($ssw_chunk)) {
        $ssw_shape = false;
    }
}
var_dump($ssw_shape);
fclose($ssw_cli);
fclose($ssw_conn);

/* And the peer that GOES AWAY. On POSIX a write to a socket whose peer has
 * closed raises SIGPIPE, whose default action ends the process — this used to
 * exit 141 with no diagnostic, because the subsystem that ignores SIGPIPE was
 * only ever started by the `-S` server. php answers false and the script keeps
 * running — and its socket ops say WHY, as a notice of their own naming the
 * count and the OS code, which is the only diagnostic the failure produces. */
$ssw_cli2 = stream_socket_client('tcp://127.0.0.1:' . $ssw_port, $ssw_e, $ssw_es, 5);
$ssw_conn2 = stream_socket_accept($ssw_srv, 5);
fclose($ssw_conn2);
$ssw_r = 0;
for ($ssw_i = 0; $ssw_i < 40; $ssw_i++) {
    $ssw_r = fwrite($ssw_cli2, 'gone');
    if ($ssw_r === false) {
        break;
    }
    usleep(20000);
}
var_dump($ssw_r);
echo "alive\n";
fclose($ssw_cli2);
fclose($ssw_srv);
?>
--EXPECTF--
int(5)
int(3)
string(8) "hellohel"
int(0)
bool(true)
bool(true)
bool(true)
%ANotice:%Afwrite(): Send of 4 bytes failed with errno=%d %s%Abool(false)
alive
--CLEAN--
<?php
unset($ssw_srv, $ssw_nm, $ssw_port, $ssw_cli, $ssw_conn, $ssw_e, $ssw_es, $ssw_chunk,
      $ssw_counts, $ssw_i, $ssw_w, $ssw_last, $ssw_shape, $ssw_c, $ssw_cli2, $ssw_conn2, $ssw_r);
