--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A first-class callable naming an unqualified function falls back to the global one
--FILE--
<?php
namespace {
    function fccFallbackTop($x) { return "top:$x"; }
}
namespace FccFallbackApp {
    function strlen($s) { return 'local-strlen'; }
    function own($x) { return "own:$x"; }
    class FccFallbackC {
        public static function m() { return 'C::m'; }
        public function i() { return 'i'; }
    }
    // the namespace has its own: it wins over the global function
    $a = strlen(...);
    echo $a("zz"), "\n";
    // an absolute name is the global one
    $b = \strlen(...);
    echo $b("zz"), "\n";
    // a namespaced user function, unqualified and fully qualified
    $c = own(...);
    echo $c(1), "\n";
    $d = \FccFallbackApp\own(...);
    echo $d(2), "\n";
    // unqualified, and only the GLOBAL namespace has it: php falls back
    $e = fccFallbackTop(...);
    echo $e(3), "\n";
    $f = count(...);
    echo $f([1, 2, 3]), "\n";
    // method/static callables are unaffected
    $g = FccFallbackC::m(...);
    echo $g(), "\n";
    $h = (new FccFallbackC)->i(...);
    echo $h(), "\n";
    // a name nobody has still names the current namespace's candidate
    try { $i = fccFallbackNope(...); }
    catch (\Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
}
?>
--EXPECT--
local-strlen
2
own:1
own:2
top:3
3
C::m
i
Error: Call to undefined function FccFallbackApp\fccFallbackNope()
--CLEAN--
<?php
