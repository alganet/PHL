--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: where a context ARRIVES — streamWrapper::$context and the transport
--FILE--
<?php
class CtxDelivery
{
    public $context;
    public function stream_open($path, $mode, $options, &$opened)
    {
        $c = $this->context;
        printf("open is_resource=%s type=%s opts=%s\n",
            var_export(is_resource($c), true),
            is_resource($c) ? get_resource_type($c) : '-',
            json_encode(is_resource($c) ? stream_context_get_options($c) : null));
        $GLOBALS['ctx_seen'] = $c;
        return true;
    }
    public function stream_read($n) { return ''; }
    public function stream_eof() { return true; }
    public function stream_stat() { return []; }
    public function stream_close() {}
}
stream_wrapper_register('ctxd', 'CtxDelivery');

/* php sets $context on the serving instance BEFORE stream_open() runs, and it
 * is ALWAYS a resource: an open that named none gets the DEFAULT context. */
echo "-- no context\n";
fclose(fopen('ctxd://x', 'r'));

echo "-- with a context\n";
$c = stream_context_create(['ctxd' => ['k' => 'v'], 'other' => ['z' => 1]]);
$h = fopen('ctxd://x', 'r', false, $c);
/* It is the SAME resource, so a later set_option is visible through it. */
var_dump($GLOBALS['ctx_seen'] === $c);
stream_context_set_option($c, 'ctxd', 'late', 1);
echo json_encode(stream_context_get_options($GLOBALS['ctx_seen'])), "\n";
fclose($h);

echo "-- file_get_contents carries it too\n";
file_get_contents('ctxd://x', false, $c);

echo "-- the default context reaches an open that named none\n";
stream_context_set_default(['ctxd' => ['d' => 1]]);
fclose(fopen('ctxd://x', 'r'));

echo "-- FILE_NO_DEFAULT_CONTEXT says do not\n";
file_get_contents('ctxd://x', FILE_NO_DEFAULT_CONTEXT);

/* php attaches the opener's context to a TRANSPORT stream and to nothing else,
 * which is what stream_context_get_options() answers from. */
echo "-- transport\n";
$srv = stream_socket_server('tcp://127.0.0.1:0');
$name = stream_socket_get_name($srv, false);
$sc = stream_context_create(['socket' => ['tcp_nodelay' => true]]);
$cl = stream_socket_client("tcp://$name", $eno, $estr, 2, STREAM_CLIENT_CONNECT, $sc);
echo 'socket: ', json_encode(stream_context_get_options($cl)), "\n";
$f = fopen(__FILE__, 'r');
echo 'file:   ', json_encode(stream_context_get_options($f)), "\n";
fclose($f);
fclose($cl);
fclose($srv);
?>
--EXPECT--
-- no context
open is_resource=true type=stream-context opts=[]
-- with a context
open is_resource=true type=stream-context opts={"ctxd":{"k":"v"},"other":{"z":1}}
bool(true)
{"ctxd":{"k":"v","late":1},"other":{"z":1}}
-- file_get_contents carries it too
open is_resource=true type=stream-context opts={"ctxd":{"k":"v","late":1},"other":{"z":1}}
-- the default context reaches an open that named none
open is_resource=true type=stream-context opts={"ctxd":{"d":1}}
-- FILE_NO_DEFAULT_CONTEXT says do not
open is_resource=true type=stream-context opts={"ctxd":{"d":1}}
-- transport
socket: {"socket":{"tcp_nodelay":true}}
file:   []
