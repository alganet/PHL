--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a failed open names the URI written and the WRAPPER's reason
--FILE--
<?php
/* php's "Failed to open stream: <reason>" says two things this engine used to
 * get wrong for every wrapper but the plain-file one.
 *
 * The NAME is the URI the script WROTE. PHL printed whatever was left after the
 * scheme came off, so a failure blamed a path nobody had written -- `fopen(x)`
 * for `data://x`, `fopen(nosuchthing)` for `php://nosuchthing`.
 *
 * The REASON is the WRAPPER's. Only the plain-file wrapper reports through
 * errno; PHL read errno for all of them, so a data:// refusal came back as
 * `Invalid argument` and a tcp:// one as `Success` -- a failed open reporting
 * that it had succeeded. php's answer is the wrapper's own sentence when it has
 * one, and a flat `operation failed` when it has not. */
set_error_handler(function ($n, $s) { echo "  ERR[$n] $s\n"; return true; });

/* The data wrapper has exactly one thing to say. */
var_dump(@fopen('data://x', 'r'));
var_dump(@file_get_contents('data://nocomma'));
var_dump(@readfile('data://;base64'));

/* php:// raises its own diagnostic BESIDE the generic line, not instead of it,
 * and the generic line's reason is the flat one. */
var_dump(@fopen('php://nosuchthing', 'r'));
var_dump(@file_get_contents('php://'));

/* A filter whose resource cannot be opened is the wrapper's failure, so the
 * reason is the flat one here too -- not the resource's errno. */
var_dump(@file_get_contents('php://filter/resource=phl_no_such_file_xyz'));

/* The plain-file wrapper is the one that DOES report errno, and it keeps its
 * scheme in the name when the script wrote one. */
var_dump(@fopen('phl_no_such_file_xyz', 'r'));
var_dump(@file_get_contents('file:///phl_no_such_file_xyz'));

/* A userland wrapper's refusal names the CALL that made it -- php reports the
 * class and method it just ran, which is the only reason there is: nothing set
 * an errno. */
class OfwW { public $context; function stream_open($p, $m, $o, &$op) { return false; } }
stream_wrapper_register('phlofw', 'OfwW');
var_dump(@fopen('phlofw://x', 'r'));
var_dump(@file_get_contents('phlofw://x'));
stream_wrapper_unregister('phlofw');

/* An open that WORKS leaves no reason behind for the next failure to pick up. */
var_dump(strlen(@file_get_contents('data://text/plain,ok')));
var_dump(@fopen('phl_no_such_file_xyz', 'r'));
?>
--EXPECT--
  ERR[2] fopen(data://x): Failed to open stream: rfc2397: no comma in URL
bool(false)
  ERR[2] file_get_contents(data://nocomma): Failed to open stream: rfc2397: no comma in URL
bool(false)
  ERR[2] readfile(data://;base64): Failed to open stream: rfc2397: no comma in URL
bool(false)
  ERR[2] fopen(): Invalid php:// URL specified
  ERR[2] fopen(php://nosuchthing): Failed to open stream: operation failed
bool(false)
  ERR[2] file_get_contents(): Invalid php:// URL specified
  ERR[2] file_get_contents(php://): Failed to open stream: operation failed
bool(false)
  ERR[2] file_get_contents(php://filter/resource=phl_no_such_file_xyz): Failed to open stream: operation failed
bool(false)
  ERR[2] fopen(phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
  ERR[2] file_get_contents(file:///phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
  ERR[2] fopen(phlofw://x): Failed to open stream: "OfwW::stream_open" call failed
bool(false)
  ERR[2] file_get_contents(phlofw://x): Failed to open stream: "OfwW::stream_open" call failed
bool(false)
int(2)
  ERR[2] fopen(phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
