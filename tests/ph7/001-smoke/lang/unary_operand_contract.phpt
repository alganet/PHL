--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unary minus/plus operand contract: php's `$x * -1` TypeError, naming int
--DESCRIPTION--
php compiles `-$x` as `$x * -1` (and `+$x` as `$x * 1`), so both operators
inherit MULTIPLICATION's operand contract, its wording included: an array,
object, resource or non-numeric string is "Unsupported operand types: P * int",
and a leading-numeric string warns and computes with the prefix. PHL warned
"could not be converted to int" and answered int(-1) — the sign of a value it
never had.
--FILE--
<?php
function uocTry($label, $fn) {
    // Build the line before echoing it so a warning lands on its own line.
    try {
        $out = var_export($fn(), true);
    } catch (TypeError $e) {
        $out = "TypeError: " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}
set_error_handler(function ($no, $msg) { echo "  Warning[$no]: $msg\n"; return true; });

class UocP {}
class UocS { public function __toString(): string { return "5"; } }

// Objects: the class name is the operand type, and __toString() does not help
// (php's multiplication never asks for a string).
uocTry('-$o', function () { $o = new UocP(); return -$o; });
uocTry('+$o', function () { $o = new UocP(); return +$o; });
uocTry('-$str_obj', function () { $o = new UocS(); return -$o; });
uocTry('-$closure', function () { $c = function () {}; return -$c; });

// Arrays and resources.
uocTry('-$a', function () { $a = [1, 2]; return -$a; });
uocTry('+$a', function () { $a = [1, 2]; return +$a; });
uocTry('-[]', fn() => -[]);
uocTry('-$res', function () { $r = fopen("php://memory", "r"); return -$r; });

// Strings: a fully numeric one is a number, a leading-numeric one warns, and
// one with no numeric prefix at all is the TypeError.
uocTry('-"12"', fn() => -"12");
uocTry('-" 12 "', fn() => -" 12 ");
uocTry('-"1e3"', fn() => -"1e3");
uocTry('-".5"', fn() => -".5");
uocTry('-"12abc"', fn() => -"12abc");
uocTry('+"12abc"', fn() => +"12abc");
uocTry('-"0x1A"', fn() => -"0x1A");
uocTry('-"abc"', fn() => -"abc");
uocTry('+"abc"', fn() => +"abc");
uocTry('-""', fn() => -"");

// null and bool coerce, as they do in multiplication.
uocTry('-null', function () { $n = null; return -$n; });
uocTry('-true', fn() => -true);
uocTry('-false', fn() => -false);

// Numbers are untouched, including the two overflow edges.
uocTry('-5', fn() => -5);
uocTry('-1.5', fn() => -1.5);
uocTry('-PHP_INT_MIN is a float', fn() => -PHP_INT_MIN === (float) PHP_INT_MAX + 1);
uocTry('-PHP_INT_MAX', fn() => -PHP_INT_MAX);
uocTry('--$x is not a decrement', function () { $x = 5; return - -$x; });

// The rejection reaches through an element and a property, and execution
// carries on after the catch.
uocTry('-$a[0]', function () { $a = [[1]]; return -$a[0]; });
uocTry('-$o->p', function () { $o = new stdClass(); $o->p = [1]; return -$o->p; });
$uocA = [1];
try { $x = -$uocA; } catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", count($uocA), " element(s)\n";
// Mid-expression: the throw must not let the rest of the expression run.
uocTry('1 + (-$a)', function () { $a = [1]; return 1 + (-$a); });
uocTry('"v" . (-$o)', function () { $o = new UocP(); return "v" . (-$o); });
restore_error_handler();
?>
--EXPECT--
-$o => TypeError: Unsupported operand types: UocP * int
+$o => TypeError: Unsupported operand types: UocP * int
-$str_obj => TypeError: Unsupported operand types: UocS * int
-$closure => TypeError: Unsupported operand types: Closure * int
-$a => TypeError: Unsupported operand types: array * int
+$a => TypeError: Unsupported operand types: array * int
-[] => TypeError: Unsupported operand types: array * int
-$res => TypeError: Unsupported operand types: resource * int
-"12" => -12
-" 12 " => -12
-"1e3" => -1000.0
-".5" => -0.5
  Warning[2]: A non-numeric value encountered
-"12abc" => -12
  Warning[2]: A non-numeric value encountered
+"12abc" => 12
  Warning[2]: A non-numeric value encountered
-"0x1A" => 0
-"abc" => TypeError: Unsupported operand types: string * int
+"abc" => TypeError: Unsupported operand types: string * int
-"" => TypeError: Unsupported operand types: string * int
-null => 0
-true => -1
-false => 0
-5 => -5
-1.5 => -1.5
-PHP_INT_MIN is a float => true
-PHP_INT_MAX => -9223372036854775807
--$x is not a decrement => 5
-$a[0] => TypeError: Unsupported operand types: array * int
-$o->p => TypeError: Unsupported operand types: array * int
caught: Unsupported operand types: array * int
still 1 element(s)
1 + (-$a) => TypeError: Unsupported operand types: array * int
"v" . (-$o) => TypeError: Unsupported operand types: UocP * int
--CLEAN--
<?php
?>
