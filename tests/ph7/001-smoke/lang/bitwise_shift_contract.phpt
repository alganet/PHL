--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`<<`/`>>`: php's operand contract, the negative-count ArithmeticError and the >= 64 saturation
--DESCRIPTION--
PHL truncated the shift COUNT to 32 bits and handed it straight to C's `<<`/`>>`,
which is undefined for a negative count or one past the operand's width: on x86
the count is masked to 6 bits, so `1 << 64` answered 1, `8 >> 64` answered 8 and
`1 << -1` answered PHP_INT_MIN. php raises `ArithmeticError: Bit shift by
negative number` and SATURATES a count of 64 or more (`<<` shifts every bit out;
`>>` keeps the sign). The operands take the same contract as the other bitwise
operators — there is no string arm for shifts, so `"abc" << 1` is
`Unsupported operand types: string << int` where PHL answered 0 — and the shift
message is positional on both sides, unlike `&`/`|`/`^`.
--FILE--
<?php
function bshTry($label, $fn) {
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ": " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}
set_error_handler(function ($no, $msg) { echo "  Warning[$no]: $msg\n"; return true; });

class BshP {}

// The ordinary shifts, and the count edges.
bshTry('1 << 0', fn() => 1 << 0);
bshTry('1 << 1', fn() => 1 << 1);
bshTry('1 << 62', fn() => 1 << 62);
bshTry('1 << 63 is PHP_INT_MIN', fn() => (1 << 63) === PHP_INT_MIN);
bshTry('1 << 64', fn() => 1 << 64);
bshTry('1 << 100', fn() => 1 << 100);
bshTry('1 << 4294967296', fn() => 1 << 4294967296);
bshTry('1 << PHP_INT_MAX', fn() => 1 << PHP_INT_MAX);
bshTry('8 >> 1', fn() => 8 >> 1);
bshTry('8 >> 64', fn() => 8 >> 64);
bshTry('-8 >> 1', fn() => -8 >> 1);
bshTry('-8 >> 65', fn() => -8 >> 65);
bshTry('-1 >> 100', fn() => -1 >> 100);
bshTry('PHP_INT_MIN >> 1', fn() => PHP_INT_MIN >> 1);
bshTry('$x=1; $x <<= 70', function () { $x = 1; $x <<= 70; return $x; });
bshTry('$x=-8; $x >>= 70', function () { $x = -8; $x >>= 70; return $x; });

// A negative count is an ArithmeticError, whatever shape it takes.
bshTry('1 << -1', fn() => 1 << -1);
bshTry('1 >> -1', fn() => 1 >> -1);
bshTry('1 >> PHP_INT_MIN', fn() => 1 >> PHP_INT_MIN);
bshTry('1 << "-2"', fn() => 1 << "-2");
bshTry('$x=1; $x <<= -1', function () { $x = 1; $x <<= -1; return $x; });
// ... and it is catchable, leaving the variable alone.
$bshX = 7;
try { $bshX <<= -1; } catch (ArithmeticError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", $bshX, "\n";
bshTry('1 + (1 << -1)', fn() => 1 + (1 << -1));

// Operands: numeric strings, bools and null coerce; there is no string shift.
bshTry('"12" << "2"', fn() => "12" << "2");
bshTry('true << 1', fn() => true << 1);
bshTry('null << 1', fn() => null << 1);
bshTry('1 << null', fn() => 1 << null);
bshTry('2.0 << 1', fn() => 2.0 << 1);
bshTry('"12abc" << 1', fn() => "12abc" << 1);
bshTry('1 << "12abc"', fn() => 1 << "12abc");
bshTry('"abc" << 1', fn() => "abc" << 1);
bshTry('1 << "abc"', fn() => 1 << "abc");
bshTry('"abc" << "1"', fn() => "abc" << "1");
bshTry('[1] >> 1', fn() => [1] >> 1);
bshTry('1 >> [1]', fn() => 1 >> [1]);
bshTry('$o << 1', function () { $o = new BshP(); return $o << 1; });
bshTry('1 << $o', function () { $o = new BshP(); return 1 << $o; });
bshTry('$res << 1', function () { $r = fopen("php://memory", "r"); return $r << 1; });
bshTry('1 << $res', function () { $r = fopen("php://memory", "r"); return 1 << $r; });
bshTry('$x="abc"; $x <<= 1', function () { $x = "abc"; $x <<= 1; return $x; });
bshTry('$x=1; $x <<= "abc"', function () { $x = 1; $x <<= "abc"; return $x; });
bshTry('$x=[1]; $x >>= 1', function () { $x = [1]; $x >>= 1; return $x; });
restore_error_handler();
?>
--EXPECT--
1 << 0 => 1
1 << 1 => 2
1 << 62 => 4611686018427387904
1 << 63 is PHP_INT_MIN => true
1 << 64 => 0
1 << 100 => 0
1 << 4294967296 => 0
1 << PHP_INT_MAX => 0
8 >> 1 => 4
8 >> 64 => 0
-8 >> 1 => -4
-8 >> 65 => -1
-1 >> 100 => -1
PHP_INT_MIN >> 1 => -4611686018427387904
$x=1; $x <<= 70 => 0
$x=-8; $x >>= 70 => -1
1 << -1 => ArithmeticError: Bit shift by negative number
1 >> -1 => ArithmeticError: Bit shift by negative number
1 >> PHP_INT_MIN => ArithmeticError: Bit shift by negative number
1 << "-2" => ArithmeticError: Bit shift by negative number
$x=1; $x <<= -1 => ArithmeticError: Bit shift by negative number
caught: Bit shift by negative number
still 7
1 + (1 << -1) => ArithmeticError: Bit shift by negative number
"12" << "2" => 48
true << 1 => 2
null << 1 => 0
1 << null => 1
2.0 << 1 => 4
  Warning[2]: A non-numeric value encountered
"12abc" << 1 => 24
  Warning[2]: A non-numeric value encountered
1 << "12abc" => 4096
"abc" << 1 => TypeError: Unsupported operand types: string << int
1 << "abc" => TypeError: Unsupported operand types: int << string
"abc" << "1" => TypeError: Unsupported operand types: string << string
[1] >> 1 => TypeError: Unsupported operand types: array >> int
1 >> [1] => TypeError: Unsupported operand types: int >> array
$o << 1 => TypeError: Unsupported operand types: BshP << int
1 << $o => TypeError: Unsupported operand types: int << BshP
$res << 1 => TypeError: Unsupported operand types: resource << int
1 << $res => TypeError: Unsupported operand types: int << resource
$x="abc"; $x <<= 1 => TypeError: Unsupported operand types: string << int
$x=1; $x <<= "abc" => TypeError: Unsupported operand types: int << string
$x=[1]; $x >>= 1 => TypeError: Unsupported operand types: array >> int
--CLEAN--
<?php
?>
