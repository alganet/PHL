--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A failed include raises php's two diagnostics and a failed require throws
--DESCRIPTION--
php answers a failed include with the stream layer's sentence -- which names the
path as written and the reason the open failed -- and then the construct's own,
which is the only one that says where it looked. This engine raised a single
sentence of its own naming neither. A failed require is not a fatal at all in
php 8: it throws an Error a script may catch, where this engine ended the run
through the uncatchable native fatal path.
--FILE--
<?php
set_include_path('.');
set_error_handler(function ($no, $msg) { echo "W: $msg\n"; return true; });

var_dump(include 'no_such_include.php');
var_dump(include_once 'no_such_include_once.php');

try {
    require 'no_such_require.php';
    echo "unreached\n";
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
echo "still running\n";

try {
    require_once 'no_such_require_once.php';
    echo "unreached\n";
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

/* A failed require is an expression that threw: the statement it sits in is
 * abandoned, not resumed. */
$n = 0;
try {
    $n = 1 + require 'no_such_expr.php';
} catch (Error $e) {
    echo "expr: ", $e->getMessage(), "\n";
}
var_dump($n);
echo "done\n";
?>
--EXPECT--
W: include(no_such_include.php): Failed to open stream: No such file or directory
W: include(): Failed opening 'no_such_include.php' for inclusion (include_path='.')
bool(false)
W: include_once(no_such_include_once.php): Failed to open stream: No such file or directory
W: include_once(): Failed opening 'no_such_include_once.php' for inclusion (include_path='.')
bool(false)
W: require(no_such_require.php): Failed to open stream: No such file or directory
Error: Failed opening required 'no_such_require.php' (include_path='.')
still running
W: require_once(no_such_require_once.php): Failed to open stream: No such file or directory
Error: Failed opening required 'no_such_require_once.php' (include_path='.')
W: require(no_such_expr.php): Failed to open stream: No such file or directory
expr: Failed opening required 'no_such_expr.php' (include_path='.')
int(0)
done
