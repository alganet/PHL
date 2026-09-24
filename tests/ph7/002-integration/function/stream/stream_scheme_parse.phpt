--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what part of a name is a SCHEME, and the file:// authority
--FILE--
<?php
set_error_handler(function ($n, $s) { echo "  ERR[$n] $s\n"; return true; });

/* php reads a scheme only at the FRONT of a name, and only as a URL scheme: a
 * run of [A-Za-z0-9+.-] at least two long followed by "://". Everything below
 * is therefore an ordinary FILENAME — including the two that carry "://" in the
 * middle and the two that carry a space where the scheme would be. */
foreach ([' php://memory', 'php ://memory', './sub://z', 'a b://c',
          'phl_no_such_dir/x://y'] as $sp_name) {
    printf("%-24s => %s\n", var_export($sp_name, true), var_export(@file_get_contents($sp_name), true));
}

/* A scheme with NOTHING after it is still a scheme: this is an unknown wrapper,
 * not a file called "phlzzz://". */
var_dump(@file_get_contents('phlzzz://'));
var_dump(@file_get_contents('phlzzz://a'));

/* A single-character run is never a scheme — which is what keeps a Windows
 * drive letter a path rather than a wrapper name. */
var_dump(@file_get_contents('q://phl_no_such'));

/* file:// has an AUTHORITY, and php reaches exactly two: the empty one and
 * `localhost`. Any other host is refused outright rather than opened as the
 * relative path that follows it. */
var_dump(@file_get_contents('file://phl_no_such_host/etc/hostname'));
var_dump(@file_get_contents('file://localhost.example/etc/hostname'));
?>
--EXPECT--
  ERR[2] file_get_contents( php://memory): Failed to open stream: No such file or directory
' php://memory'          => false
  ERR[2] file_get_contents(php ://memory): Failed to open stream: No such file or directory
'php ://memory'          => false
  ERR[2] file_get_contents(./sub://z): Failed to open stream: No such file or directory
'./sub://z'              => false
  ERR[2] file_get_contents(a b://c): Failed to open stream: No such file or directory
'a b://c'                => false
  ERR[2] file_get_contents(phl_no_such_dir/x://y): Failed to open stream: No such file or directory
'phl_no_such_dir/x://y'  => false
  ERR[2] file_get_contents(): Unable to find the wrapper "phlzzz" - did you forget to enable it when you configured PHP?
  ERR[2] file_get_contents(phlzzz://): Failed to open stream: No such file or directory
bool(false)
  ERR[2] file_get_contents(): Unable to find the wrapper "phlzzz" - did you forget to enable it when you configured PHP?
  ERR[2] file_get_contents(phlzzz://a): Failed to open stream: No such file or directory
bool(false)
  ERR[2] file_get_contents(q://phl_no_such): Failed to open stream: No such file or directory
bool(false)
  ERR[2] file_get_contents(): Remote host file access not supported, file://phl_no_such_host/etc/hostname
  ERR[2] file_get_contents(file://phl_no_such_host/etc/hostname): Failed to open stream: no suitable wrapper could be found
bool(false)
  ERR[2] file_get_contents(): Remote host file access not supported, file://localhost.example/etc/hostname
  ERR[2] file_get_contents(file://localhost.example/etc/hostname): Failed to open stream: no suitable wrapper could be found
bool(false)
--CLEAN--
<?php
unset($sp_name);
