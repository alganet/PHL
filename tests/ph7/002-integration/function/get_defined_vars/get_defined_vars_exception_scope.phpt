--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_vars() reports the enclosing scope from inside try/catch/finally
--FILE--
<?php
function gdvNested()
{
    $a = 1;
    try {
        $b = 2;
        try {
            $c = 3;
            echo "nested-try: ", json_encode(array_keys(get_defined_vars())), "\n";
        } catch (Throwable $e) {
        }
        try {
            throw new RuntimeException('x');
        } catch (RuntimeException $e) {
            $d = 4;
            echo "catch: ", json_encode(array_keys(get_defined_vars())), "\n";
            try {
                echo "try-in-catch: ", json_encode(array_keys(get_defined_vars())), "\n";
            } finally {
            }
        } finally {
            echo "finally: ", json_encode(array_keys(get_defined_vars())), "\n";
        }
    } finally {
    }
}
gdvNested();

class GdvProbe
{
    public function m($p)
    {
        try {
            $q = 1;
            echo "method-try: ", json_encode(array_keys(get_defined_vars())), "\n";
        } catch (Throwable $e) {
        }
    }
}
(new GdvProbe)->m('P');

$gdvClosure = function ($u) {
    try {
        $w = 1;
        echo "closure-try: ", json_encode(array_keys(get_defined_vars())), "\n";
    } finally {
    }
};
$gdvClosure('U');

function gdvGen()
{
    $g = 1;
    try {
        yield 1;
        echo "gen-try: ", json_encode(array_keys(get_defined_vars())), "\n";
    } finally {
    }
}
foreach (gdvGen() as $ignored) {
}

function gdvGlobalImport()
{
    global $gdvOuter;
    try {
        echo "global-import-try: ", json_encode(array_keys(get_defined_vars())), "\n";
    } finally {
    }
}
$gdvOuter = 9;
gdvGlobalImport();

// At the GLOBAL scope the superglobals are part of the answer, inside a try too.
try {
    $vars = get_defined_vars();
    echo "global-try: gdvOuter=", (int) isset($vars['gdvOuter']),
         " _SERVER=", (int) isset($vars['_SERVER']), "\n";
} finally {
}
?>
--EXPECT--
nested-try: ["a","b","c"]
catch: ["a","b","c","e","d"]
try-in-catch: ["a","b","c","e","d"]
finally: ["a","b","c","e","d"]
method-try: ["p","q"]
closure-try: ["u","w"]
gen-try: ["g"]
global-import-try: ["gdvOuter"]
global-try: gdvOuter=1 _SERVER=1
--CLEAN--
<?php
