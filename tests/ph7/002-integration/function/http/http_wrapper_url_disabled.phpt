--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: allow_url_fopen=0 takes it out of service
--DESCRIPTION--
`allow_url_fopen` is the wholesale switch over every URL wrapper, and php makes
it PHP_INI_SYSTEM -- a script cannot turn it back on -- so this needs its own
run with the directive already off. Nothing is opened and no socket is made, and
php says it TWICE: its own sentence naming the wrapper and the directive, and
then the caller's, which reports an open that found no wrapper at all. This
engine used to raise only the first and let the caller print whatever reason was
armed for the wrapper ("operation failed").

The wrapper is still LISTED: php reports what is registered, not what the
configuration will let a script use.
--SKIPIF--
<?php
if (!in_array('http', stream_get_wrappers(), true)) {
    echo "skip no http:// wrapper in this build";
}
?>
--INI--
allow_url_fopen=0
--FILE--
<?php
var_dump(in_array('http', stream_get_wrappers(), true));
var_dump(ini_get('allow_url_fopen'));
$seen = array();
set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
var_dump(file_get_contents('http://127.0.0.1:1/x'));
var_dump(fopen('http://127.0.0.1:1/x', 'r'));
restore_error_handler();
foreach ($seen as $line) {
    echo $line, "\n";
}
/* A script cannot lift it. */
var_dump(ini_set('allow_url_fopen', '1'));
var_dump(@file_get_contents('http://127.0.0.1:1/x'));
?>
--EXPECT--
bool(true)
string(1) "0"
bool(false)
bool(false)
file_get_contents(): http:// wrapper is disabled in the server configuration by allow_url_fopen=0
file_get_contents(http://127.0.0.1:1/x): Failed to open stream: no suitable wrapper could be found
fopen(): http:// wrapper is disabled in the server configuration by allow_url_fopen=0
fopen(http://127.0.0.1:1/x): Failed to open stream: no suitable wrapper could be found
bool(false)
bool(false)
