--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a C-bodied method enforces its declared arity, both bounds, like an internal function
--FILE--
<?php
/* A method declared in an embedded PHP chunk got NO central arity enforcement --
 * the too-many-arguments ArgumentCountError covered prelude FUNCTIONS only -- so
 * `$c->bindTo(null,'static','extra')` silently ignored the extra argument. A
 * native method declares one PHP-style signature string, and that single string
 * is the source of its minimum, its maximum and its by-ref positions, exactly as
 * aBuiltinSig[] is for a builtin. Both bounds now fire, with php's wording. */
$c = function () {};
$cases = [
    'too few (instance)'  => fn () => $c->bindTo(),
    'too many (instance)' => fn () => $c->bindTo(null, 'static', 'extra'),
    'too few (static)'    => fn () => Closure::bind($c),
    'too many (static)'   => fn () => Closure::bind($c, null, 'static', 'extra'),
    'fromCallable 0'      => fn () => Closure::fromCallable(),
];
foreach ($cases as $label => $t) {
    try {
        $t();
        echo "$label: no error\n";
    } catch (Throwable $e) {
        echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
too few (instance): ArgumentCountError: Closure::bindTo() expects at least 1 argument, 0 given
too many (instance): ArgumentCountError: Closure::bindTo() expects at most 2 arguments, 3 given
too few (static): ArgumentCountError: Closure::bind() expects at least 2 arguments, 1 given
too many (static): ArgumentCountError: Closure::bind() expects at most 3 arguments, 4 given
fromCallable 0: ArgumentCountError: Closure::fromCallable() expects exactly 1 argument, 0 given
--CLEAN--
<?php
