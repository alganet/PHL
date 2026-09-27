--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two transfers driven together by a multi handle, over a real socket
--DESCRIPTION--
The multi loop as a program writes it -- exec, select, exec until nothing is
still running, then drain the message queue -- against the local test server,
which is the only way to see the answers a set gives about transfers that
really ran.

What only a real pair of transfers shows:

  * the message queue reports each transfer's own CURLcode with the SAME
    CurlHandle object that was added, and counts down the ones still queued.
    A failure and a success are both messages: the 404 is a successful
    transfer, and the connection that could not be made is CURLE_COULDNT_CONNECT
    beside it;

  * curl_multi_info_read() is what puts the result ON the handle. Until the
    message is read, curl_errno() on a handle whose transfer has finished still
    answers 0 and curl_error() the empty string;

  * every body destination works under a multi exactly as under curl_exec:
    CURLOPT_RETURNTRANSFER collects (and curl_multi_getcontent() is how the
    bytes are reached afterwards), the default prints through the script's own
    output so ob_start() catches it, and a WRITEFUNCTION is called from inside
    the loop;

  * a handle that is in a set cannot be curl_exec()'d -- libcurl refuses with
    its own sentence -- and re-adding one for a second transfer empties the
    collected body first.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
}
?>
--FILE--
<?php
require __DIR__ . '/http_server.inc';

$port = curl_test_server_start($proc, 19740);
if ($port === null) {
    echo "server did not start\n";
    return;
}
$base = 'http://127.0.0.1:' . $port;

/* the loop every program writes; answers the number of turns it took */
function multiDrive($mh)
{
    $still = null;
    $turns = 0;
    do {
        $rc = curl_multi_exec($mh, $still);
        $turns++;
        if ($still > 0) {
            curl_multi_select($mh, 1.0);
        }
    } while ($still > 0 && $rc === CURLM_OK && $turns < 200);
    return $rc;
}

$mh = curl_multi_init();
$ok = curl_init($base . '/status/200');
curl_setopt($ok, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ok, CURLOPT_TIMEOUT, 10);
$dead = curl_init('http://127.0.0.1:1/');
curl_setopt($dead, CURLOPT_RETURNTRANSFER, true);
curl_setopt($dead, CURLOPT_TIMEOUT, 10);
curl_multi_add_handle($mh, $ok);
curl_multi_add_handle($mh, $dead);

printf("-- before the loop: errno=%d error=%s\n", curl_errno($dead), var_export(curl_error($dead), true));
printf("-- loop: rc=%d\n", multiDrive($mh));
printf("-- still no result on the handle: errno=%d error=%s\n", curl_errno($dead),
    var_export(curl_error($dead), true));

/* the queue: one message per transfer, in completion order, so they are sorted
   here rather than pinned */
$seen = array();
$counts = array();
$queued = -1;
while (($msg = curl_multi_info_read($mh, $queued)) !== false) {
    $which = $msg['handle'] === $ok ? 'ok' : ($msg['handle'] === $dead ? 'dead' : '?');
    $seen[$which] = sprintf("msg=%d result=%d", $msg['msg'], $msg['result']);
    $counts[] = $queued;
}
ksort($seen);
foreach ($seen as $which => $line) {
    printf("-- message for %-4s %s\n", $which, $line);
}
/* the completion ORDER is the library's, so only the count itself is pinned */
rsort($counts);
printf("-- queued counted down: %s\n", implode(',', $counts));
printf("-- and now the handle has it: errno=%d error=%s\n", curl_errno($dead),
    var_export(curl_error($dead) !== '', true));
printf("-- the successful one: errno=%d code=%d content=%s\n", curl_errno($ok),
    curl_getinfo($ok, CURLINFO_HTTP_CODE), var_export(curl_multi_getcontent($ok), true));
var_dump(curl_multi_info_read($mh));

/* a handle in a set is libcurl's own refusal, and it empties the body */
printf("-- exec inside the set: %s errno=%d error=%s content=%s\n",
    var_export(curl_exec($ok), true), curl_errno($ok), var_export(curl_error($ok), true),
    var_export(curl_multi_getcontent($ok), true));

/* the other two destinations, driven by the same loop */
$m2 = curl_multi_init();
$printed = curl_init($base . '/status/201');
curl_setopt($printed, CURLOPT_TIMEOUT, 10);
$called = curl_init($base . '/status/202');
curl_setopt($called, CURLOPT_TIMEOUT, 10);
$chunks = '';
curl_setopt($called, CURLOPT_WRITEFUNCTION, function ($h, $c) use (&$chunks) {
    $chunks .= $c;
    return strlen($c);
});
curl_multi_add_handle($m2, $printed);
curl_multi_add_handle($m2, $called);
ob_start();
multiDrive($m2);
$out = ob_get_clean();
printf("-- printed=%s called=%s getcontent(printed)=%s\n", var_export($out, true),
    var_export($chunks, true), var_export(curl_multi_getcontent($printed), true));

/* re-adding a handle empties what it collected */
$m3 = curl_multi_init();
$again = curl_init($base . '/status/200');
curl_setopt($again, CURLOPT_RETURNTRANSFER, true);
curl_setopt($again, CURLOPT_TIMEOUT, 10);
curl_multi_add_handle($m3, $again);
multiDrive($m3);
while (curl_multi_info_read($m3) !== false) {
}
printf("-- first: %s\n", var_export(curl_multi_getcontent($again), true));
curl_multi_remove_handle($m3, $again);
printf("-- kept after the remove: %s\n", var_export(curl_multi_getcontent($again), true));
curl_setopt($again, CURLOPT_URL, $base . '/status/404');
curl_multi_add_handle($m3, $again);
printf("-- emptied by the re-add: %s\n", var_export(curl_multi_getcontent($again), true));
multiDrive($m3);
printf("-- second: %s\n", var_export(curl_multi_getcontent($again), true));

/* a callback that throws under the loop: the exception travels out of
   curl_multi_exec, and the transfer it threw out of still finished */
$m4 = curl_multi_init();
$boom = curl_init($base . '/status/200');
curl_setopt($boom, CURLOPT_TIMEOUT, 10);
curl_setopt($boom, CURLOPT_WRITEFUNCTION, function ($h, $c) { throw new RuntimeException('from the loop'); });
curl_multi_add_handle($m4, $boom);
$still = null;
$caught = '';
for ($i = 0; $i < 20; $i++) {
    try {
        curl_multi_exec($m4, $still);
    } catch (Throwable $e) {
        $caught = get_class($e) . ': ' . $e->getMessage();
    }
    if ($still > 0) {
        curl_multi_select($m4, 1.0);
    } else {
        break;
    }
}
$msg = curl_multi_info_read($m4);
printf("-- throwing under the loop: %s still=%d result=%d size=%d code=%d\n", $caught, $still,
    $msg['result'], curl_getinfo($boom, CURLINFO_SIZE_DOWNLOAD),
    curl_getinfo($boom, CURLINFO_HTTP_CODE));

curl_test_server_stop($proc, $port);
?>
--EXPECT--
-- before the loop: errno=0 error=''
-- loop: rc=0
-- still no result on the handle: errno=0 error=''
-- message for dead msg=1 result=7
-- message for ok   msg=1 result=0
-- queued counted down: 1,0
-- and now the handle has it: errno=7 error=true
-- the successful one: errno=0 code=200 content='status 200'
bool(false)
-- exec inside the set: false errno=2 error='easy handle already used in multi handle' content=''
-- printed='status 201' called='status 202' getcontent(printed)=NULL
-- first: 'status 200'
-- kept after the remove: 'status 200'
-- emptied by the re-add: ''
-- second: 'status 404'
-- throwing under the loop: RuntimeException: from the loop still=0 result=0 size=10 code=200
--CLEAN--
<?php
unset($port, $base, $proc, $mh, $m2, $m3, $m4, $ok, $dead, $seen, $counts, $queued, $msg,
    $printed, $called, $chunks, $out, $again, $boom, $still, $caught, $which, $line, $i);
?>
