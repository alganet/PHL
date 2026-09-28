--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
libxml_set_streams_context() reaches a DOM load over http://
--DESCRIPTION--
`libxml_set_streams_context()` stores the context php's document loaders open
with, and until there was an http:// wrapper to read it the slot was stored,
answered, and read by NOTHING -- there was no open it could change.

There is now: `DOMDocument::load('http://…')` composes its request through the
same wrapper `file_get_contents()` does, so the context's `header` and
`user_agent` ride along. A file:// load ignores it, exactly as php's does.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (!in_array('http', stream_get_wrappers(), true)) {
    echo "skip no http:// wrapper in this build";
} elseif (!class_exists('DOMDocument') || !function_exists('libxml_set_streams_context')) {
    echo "skip needs ext/dom and ext/libxml";
}
?>
--FILE--
<?php
require __DIR__ . '/http_wrapper_server.inc';

$dir = http_test_dir('lxc');
$port = http_test_server_start($proc, $dir);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'http://127.0.0.1:' . $port;
$doc = "<r><a>1</a></r>";
http_test_reply($dir, 'xml', "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\n"
    . 'Content-Length: ' . strlen($doc) . "\r\nConnection: close\r\n\r\n" . $doc);

/* 1. no context: the wrapper's own request */
http_test_requests_clear($dir);
$d = new DOMDocument();
printf("-- plain load: %s\n", var_export(@$d->load("$base/canned/xml"), true));
echo trim($d->saveXML()), "\n";
$reqs = http_test_requests($dir);
echo http_test_scrub($reqs[0]), "|\n";

/* 2. with one: the headers and the user agent it carries reach the wire */
libxml_set_streams_context(stream_context_create(array('http' => array(
    'header' => 'X-Ctx: yes',
    'user_agent' => 'DomUA/1',
))));
http_test_requests_clear($dir);
$e = new DOMDocument();
printf("-- with a context: %s\n", var_export(@$e->load("$base/canned/xml"), true));
$reqs = http_test_requests($dir);
echo http_test_scrub($reqs[0]), "|\n";

/* 3. a file:// load ignores it and still reads */
$tmp = tempnam(sys_get_temp_dir(), 'lxc');
file_put_contents($tmp, $doc);
$f = new DOMDocument();
printf("-- a local file: %s / %s\n", var_export(@$f->load($tmp), true),
    trim($f->saveXML()) === trim($d->saveXML()) ? 'same document' : 'different');
unlink($tmp);

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECT--
-- plain load: true
<?xml version="1.0"?>
<r><a>1</a></r>
GET /canned/xml HTTP/1.1
Host: HOST
Connection: close

|
-- with a context: true
GET /canned/xml HTTP/1.1
Host: HOST
Connection: close
User-Agent: DomUA/1
X-Ctx: yes

|
-- a local file: true / same document
