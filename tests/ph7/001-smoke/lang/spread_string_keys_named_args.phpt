--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Argument unpacking binds string keys as PHP 8.1 named arguments
--FILE--
<?php
/* PHP 8.1: when `...$arr` is unpacked into a call, integer keys pass
 * positionally and string keys bind as named arguments (by parameter name,
 * order-independent, filling the variadic with their keys). A spread that
 * expands to !=1 element also keeps any following named/positional args aligned.
 * Verified byte-identical to php 8.5.7 across every call kind. */

function sskFn($a = 0, $b = 0, $c = 0) { return "$a-$b-$c"; }

// Pure spread: string key -> named, order-independent.
echo sskFn(...["b" => 2]), "\n";               // 0-2-0
echo sskFn(...["c" => 3, "a" => 1]), "\n";     // 1-0-3
echo sskFn(...["c" => 9, "b" => 8, "a" => 7]), "\n"; // 7-8-9

// Positional + spread named.
echo sskFn(1, ...["c" => 3]), "\n";            // 1-0-3

// A spread expanding to 2 keeps a following compile-time named arg aligned.
echo sskFn(...[1, 2], c: 9), "\n";             // 1-2-9

// An empty spread before a named arg stays aligned.
echo sskFn(...[], c: 5), "\n";                 // 0-0-5

// Two spreads, both named.
echo sskFn(...["a" => 1], ...["c" => 3]), "\n"; // 1-0-3

// Variadic collector keeps the string keys.
function sskVar($x, ...$rest) { return $x . ":" . json_encode($rest); }
echo sskVar(...["x" => 1, "y" => 2, "z" => 3]), "\n"; // 1:{"y":2,"z":3}
echo sskVar(1, ...["k" => 9]), "\n";                   // 1:{"k":9}

// Methods (instance + static), closures, arrow fns, constructors.
class SskC {
    public function m($a = 0, $b = 0) { return "$a$b"; }
    public static function s($a = 0, $b = 0) { return "$a$b"; }
}
echo (new SskC)->m(...["b" => 7]), "\n";       // 07
echo SskC::s(...["b" => 7, "a" => 3]), "\n";   // 37
$sskClosure = function ($a = 0, $b = 0) { return "$a$b"; };
echo $sskClosure(...["b" => 9]), "\n";         // 09
$sskArrow = fn($a = 0, $b = 0) => "$a$b";
echo $sskArrow(...["b" => 4]), "\n";           // 04

class SskCtor {
    public $a; public $b; public $c;
    public function __construct($a = 0, $b = 0, $c = 0) { $this->a = $a; $this->b = $b; $this->c = $c; }
}
$o = new SskCtor(...["c" => 3, "a" => 1]);
echo "{$o->a}{$o->b}{$o->c}", "\n";            // 103

// Array callables and call_user_func forward the keys.
$sskArr = [new SskC, "m"];
echo $sskArr(...["b" => 5]), "\n";             // 05
echo call_user_func("sskFn", ...["b" => 2]), "\n"; // 0-2-0

// Plain int-key spread is unchanged (positional).
echo sskFn(...[1, 2, 3]), "\n";                // 1-2-3

// Nested spread-bearing calls must keep per-call scope: an inner unpack call in a
// named/spread argument of an outer unpack call must not corrupt the outer binding.
function sskInner($b = 0) { return $b * 10; }
echo sskFn(...["a" => 1], c: sskInner(...["b" => 2])), "\n"; // 1-0-20
function sskMk($b = 0) { return []; }
echo sskFn(...["b" => 5], ...sskMk(...["b" => 9])), "\n";    // 0-5-0
?>
--EXPECT--
0-2-0
1-0-3
7-8-9
1-0-3
1-2-9
0-0-5
1-0-3
1:{"y":2,"z":3}
1:{"k":9}
07
37
09
04
103
05
0-2-0
1-2-3
1-0-20
0-5-0
--CLEAN--
<?php
