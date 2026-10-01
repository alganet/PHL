--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An include of a scheme no wrapper answers to warns and falls back to plain files
--DESCRIPTION--
php's stream lookup does not refuse an unregistered scheme: it warns, forgets the
protocol and hands the WHOLE uri to the plain-files wrapper. So the failure a
script reads is that wrapper's -- "No such file or directory" -- preceded by one
"Unable to find the wrapper" sentence per lookup that still saw the scheme. php
resolves the path before it opens it, and the _once forms resolve it once more to
answer whether it has already been included, so a miss costs two sentences and a
_once miss three. This engine reported the raw "Invalid argument" of an open it
never attempted, and said nothing about the wrapper at all. A file:// with an
authority is the one scheme php words differently: a wrapper WAS found and
declined the name.
--FILE--
<?php
set_include_path('.');
set_error_handler(function ($no, $msg) { echo "W: $msg\n"; return true; });

var_dump(include 'zzz://nope.php');
var_dump(include_once 'zzz://nope.php');

try {
    require 'zzz://nope.php';
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    require_once 'zzz://nope.php';
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

/* The scheme is echoed exactly as the script wrote it. */
var_dump(include 'ZzZ://nope.php');

/* A wrapper that WAS found and declined the name is php's other sentence. */
var_dump(include 'file://host/nope.php');

echo "done\n";
?>
--EXPECT--
W: include(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: include(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: include(zzz://nope.php): Failed to open stream: No such file or directory
W: include(): Failed opening 'zzz://nope.php' for inclusion (include_path='.')
bool(false)
W: include_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: include_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: include_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: include_once(zzz://nope.php): Failed to open stream: No such file or directory
W: include_once(): Failed opening 'zzz://nope.php' for inclusion (include_path='.')
bool(false)
W: require(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: require(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: require(zzz://nope.php): Failed to open stream: No such file or directory
Error: Failed opening required 'zzz://nope.php' (include_path='.')
W: require_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: require_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: require_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: require_once(zzz://nope.php): Failed to open stream: No such file or directory
Error: Failed opening required 'zzz://nope.php' (include_path='.')
W: include(): Unable to find the wrapper "ZzZ" - did you forget to enable it when you configured PHP?
W: include(): Unable to find the wrapper "ZzZ" - did you forget to enable it when you configured PHP?
W: include(ZzZ://nope.php): Failed to open stream: No such file or directory
W: include(): Failed opening 'ZzZ://nope.php' for inclusion (include_path='.')
bool(false)
W: include(): Remote host file access not supported, file://host/nope.php
W: include(file://host/nope.php): Failed to open stream: no suitable wrapper could be found
W: include(): Failed opening 'file://host/nope.php' for inclusion (include_path='.')
bool(false)
done
