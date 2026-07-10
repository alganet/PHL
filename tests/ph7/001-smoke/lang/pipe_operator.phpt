--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8.5 pipe operator |>: $x |> f(...) calls f($x), left-associative
--FILE--
<?php
// chained first-class callables
echo "  hello  " |> trim(...) |> strtoupper(...), "\n";
// closure on the right
$double = fn($x) => $x * 2;
echo 5 |> $double, "\n";
// callable string
echo "hello" |> 'strlen', "\n";
// builtin taking one array arg
echo [3, 1, 2] |> array_sum(...), "\n";
// method / static callables
class M {
    function d($x) { return $x * 2; }
    static function inc($x) { return $x + 1; }
}
$o = new M();
echo 3 |> $o->d(...), "\n";
echo 5 |> M::inc(...), "\n";
echo 4 |> [$o, "d"], "\n";
// precedence: binds looser than + and . , tighter than == and ??
echo 2 + 3 |> $double, "\n";              // (2+3) |> double = 10
echo "a" . "b" |> strtoupper(...), "\n";  // (ab) |> upper = AB
echo (3 |> $double == 6) ? "yes" : "no", "\n"; // (3|>double) == 6 => true
$n = 7;
echo $n ?? 99 |> $double, "\n";           // $n ?? (99|>double) => 7
// used inside a larger expression
echo (10 |> $double) + 1, "\n";
?>
--EXPECT--
HELLO
10
5
6
6
6
8
10
AB
yes
7
21
--CLEAN--
<?php
