--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: registered, read-only, and its two ini directives
--DESCRIPTION--
The parts of the wrapper's surface that need nothing on the other end of a
socket: it is REGISTERED (which is what `file_get_contents('http://...')`
resolving depends on), it declines every mode that could write BEFORE it opens
anything -- php's screen there is TEXTUAL, `strpbrk(mode, "awx+")`, which is
why `c` is the one write-ish mode it lets through and answers for by trying the
connection -- and the two ini directives it reads exist with php's defaults.

php 8.4's `http_get_last_response_headers()` and
`http_clear_last_response_headers()` are here too, for the half of them that
needs no exchange: the getter is a `?array` and answers NULL when nothing has
been recorded, and the clear answers nothing at all.

The exchange itself is in 002-integration/function/http, where a server can be
spawned.
--FILE--
<?php
$hwsWrappers = stream_get_wrappers();
var_dump(in_array('http', $hwsWrappers, true));

/* Refused before a socket is made: port 1 is never listening, and the reason
 * is the mode rather than the connection. */
$hwsSeen = array();
set_error_handler(function ($no, $str) use (&$hwsSeen) { $hwsSeen[] = $str; return true; });
foreach (array('w', 'a', 'r+', 'x', 'c') as $hwsMode) {
    $hwsSeen = array();
    $hwsH = fopen('http://127.0.0.1:1/x', $hwsMode);
    /* The REASON is what is under test, and only the mode's own is portable:
     * what a refused connection is called is the platform's. */
    $hwsWhy = 'the connection';
    foreach ($hwsSeen as $hwsLine) {
        if (strpos($hwsLine, 'does not support writeable connections') !== false) {
            $hwsWhy = 'the mode';
        }
    }
    printf("%s: %s / refused by %s\n", $hwsMode, var_export($hwsH, true), $hwsWhy);
}
restore_error_handler();

/* php 8.4's two last-response-header functions, which read the same store
 * $http_response_header is written from. Nothing has been fetched here, so the
 * getter answers NULL -- and it is a ?array, never the empty one. */
http_clear_last_response_headers();
var_dump(http_get_last_response_headers());
var_dump(http_clear_last_response_headers());
var_dump(http_get_last_response_headers());

/* The two directives the request composer reads. php ships user_agent as the
 * empty string and `from` with no value at all, which ini_get() answers alike. */
var_dump(ini_get('user_agent'), ini_get('from'));
$hwsOld = ini_set('user_agent', 'probe/1');
var_dump($hwsOld, ini_get('user_agent'));
ini_set('user_agent', $hwsOld);
var_dump(ini_get('user_agent'));
?>
--EXPECT--
bool(true)
w: false / refused by the mode
a: false / refused by the mode
r+: false / refused by the mode
x: false / refused by the mode
c: false / refused by the connection
NULL
NULL
NULL
string(0) ""
string(0) ""
string(0) ""
string(7) "probe/1"
string(0) ""
