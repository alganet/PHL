--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`&`/`|`/`^` over two strings are per-byte ops; anything else takes php's operand contract
--DESCRIPTION--
php's `&`, `|` and `^` have a STRING arm PHL did not implement at all: two string
operands are combined BYTE BY BYTE and the result is a binary string
(`"abc" & "abd"` is "ab`"), where PHL cast both sides to int and answered int(0)
— so every XOR-mask / bit-blend idiom silently produced a number. `&` and `^`
stop at the shorter operand; `|` runs to the longer one, so its tail is copied
verbatim. Every other operand pair takes the ARITHMETIC operand contract with
this operator's name (`Unsupported operand types: array & int`), where PHL cast
arrays, objects and resources to integers and answered a number.
--FILE--
<?php
function bsocTry($label, $fn) {
    try {
        $r = $fn();
        $out = is_string($r) ? "string(" . strlen($r) . ") [" . bin2hex($r) . "]" : var_export($r, true);
    } catch (TypeError $e) {
        $out = "TypeError: " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}
set_error_handler(function ($no, $msg) { echo "  Warning[$no]: $msg\n"; return true; });

class BsocP {}
class BsocS { public function __toString(): string { return "ab"; } }

// Two strings: per byte, and the length rule differs between & ^ and |.
bsocTry('"abc" & "abd"', fn() => "abc" & "abd");
bsocTry('"abc" | "ABD"', fn() => "abc" | "ABD");
bsocTry('"abc" ^ "abd"', fn() => "abc" ^ "abd");
bsocTry('"abc" & "ab"', fn() => "abc" & "ab");
bsocTry('"ab" & "abc"', fn() => "ab" & "abc");
bsocTry('"abc" | "ab"', fn() => "abc" | "ab");
bsocTry('"ab" | "abc"', fn() => "ab" | "abc");
bsocTry('"abc" ^ "ab"', fn() => "abc" ^ "ab");
bsocTry('"" & "abc"', fn() => "" & "abc");
bsocTry('"" | "abc"', fn() => "" | "abc");
bsocTry('"" & ""', fn() => "" & "");
// Binary-safe, and NUMERIC strings are strings too (this is the pair PHL got
// closest to by accident: "12" & "13" answered int(12), not the string "12").
bsocTry('"12" & "13"', fn() => "12" & "13");
bsocTry('"12" & "3"', fn() => "12" & "3");
bsocTry('"\x00\xff" ^ "\xff\x00"', fn() => "\x00\xff" ^ "\xff\x00");
bsocTry('one-byte offsets', function () { $s = "abc"; return $s[0] & "a"; });
bsocTry('nested ("a"|"b")&"c"', fn() => ("a" | "b") & "c");
bsocTry('$x="abc"; $x |= "AB"', function () { $x = "abc"; $x |= "AB"; return $x; });
bsocTry('$x="abc"; $x ^= "abc"', function () { $x = "abc"; $x ^= "abc"; return $x; });
bsocTry('$x="ab"; $x |= "ABC"', function () { $x = "ab"; $x |= "ABC"; return $x; });

// One string and one number: the numeric arm, as before.
bsocTry('"12" & 3', fn() => "12" & 3);
bsocTry('3 & "12"', fn() => 3 & "12");
bsocTry('$x=5; $x &= "3"', function () { $x = 5; $x &= "3"; return $x; });
bsocTry('3 & "12abc"', fn() => 3 & "12abc");
bsocTry('null & 1', fn() => null & 1);
bsocTry('true & 1', fn() => true & 1);
bsocTry('null & null', fn() => null & null);
bsocTry('true ^ false', fn() => true ^ false);
bsocTry('6 & 3', fn() => 6 & 3);

// A non-numeric string against a non-string, and every non-scalar: TypeError.
bsocTry('"abc" & 1', fn() => "abc" & 1);
bsocTry('1 | "abc"', fn() => 1 | "abc");
bsocTry('null & "abc"', fn() => null & "abc");
bsocTry('"abc" ^ null', fn() => "abc" ^ null);
bsocTry('true & "abc"', fn() => true & "abc");
bsocTry('[1] & 1', fn() => [1] & 1);
bsocTry('1 & [1]', fn() => 1 & [1]);
bsocTry('[1] & "a"', fn() => [1] & "a");
bsocTry('$o | 1', function () { $o = new BsocP(); return $o | 1; });
bsocTry('$str_obj & "ab"', function () { $o = new BsocS(); return $o & "ab"; });
bsocTry('"ab" & $str_obj', function () { $o = new BsocS(); return "ab" & $o; });
bsocTry('$o ^ $o2', function () { return (new BsocP()) ^ (new BsocP()); });
bsocTry('$res & 1', function () { $r = fopen("php://memory", "r"); return $r & 1; });
bsocTry('1 & $res', function () { $r = fopen("php://memory", "r"); return 1 & $r; });
bsocTry('$res & $o', function () { $r = fopen("php://memory", "r"); return $r & (new BsocP()); });
bsocTry('$a = [1]; $a |= 1', function () { $a = [1]; $a |= 1; return $a; });
bsocTry('$x = 1; $x &= $o', function () { $x = 1; $x &= new BsocP(); return $x; });

// The operands are untouched and execution carries on after the catch.
$bsocA = [1, 2];
try { $x = $bsocA & 1; } catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", count($bsocA), " element(s)\n";
bsocTry('1 + ([1] & 1)', function () { $a = [1]; return 1 + ($a & 1); });
restore_error_handler();
?>
--EXPECT--
"abc" & "abd" => string(3) [616260]
"abc" | "ABD" => string(3) [616267]
"abc" ^ "abd" => string(3) [000007]
"abc" & "ab" => string(2) [6162]
"ab" & "abc" => string(2) [6162]
"abc" | "ab" => string(3) [616263]
"ab" | "abc" => string(3) [616263]
"abc" ^ "ab" => string(2) [0000]
"" & "abc" => string(0) []
"" | "abc" => string(3) [616263]
"" & "" => string(0) []
"12" & "13" => string(2) [3132]
"12" & "3" => string(1) [31]
"\x00\xff" ^ "\xff\x00" => string(2) [ffff]
one-byte offsets => string(1) [61]
nested ("a"|"b")&"c" => string(1) [63]
$x="abc"; $x |= "AB" => string(3) [616263]
$x="abc"; $x ^= "abc" => string(3) [000000]
$x="ab"; $x |= "ABC" => string(3) [616243]
"12" & 3 => 0
3 & "12" => 0
$x=5; $x &= "3" => 1
  Warning[2]: A non-numeric value encountered
3 & "12abc" => 0
null & 1 => 0
true & 1 => 1
null & null => 0
true ^ false => 1
6 & 3 => 2
"abc" & 1 => TypeError: Unsupported operand types: string & int
1 | "abc" => TypeError: Unsupported operand types: int | string
null & "abc" => TypeError: Unsupported operand types: null & string
"abc" ^ null => TypeError: Unsupported operand types: string ^ null
true & "abc" => TypeError: Unsupported operand types: bool & string
[1] & 1 => TypeError: Unsupported operand types: array & int
1 & [1] => TypeError: Unsupported operand types: int & array
[1] & "a" => TypeError: Unsupported operand types: array & string
$o | 1 => TypeError: Unsupported operand types: BsocP | int
$str_obj & "ab" => TypeError: Unsupported operand types: BsocS & string
"ab" & $str_obj => TypeError: Unsupported operand types: BsocS & string
$o ^ $o2 => TypeError: Unsupported operand types: BsocP ^ BsocP
$res & 1 => TypeError: Unsupported operand types: resource & int
1 & $res => TypeError: Unsupported operand types: resource & int
$res & $o => TypeError: Unsupported operand types: resource & BsocP
$a = [1]; $a |= 1 => TypeError: Unsupported operand types: array | int
$x = 1; $x &= $o => TypeError: Unsupported operand types: int & BsocP
caught: Unsupported operand types: array & int
still 2 element(s)
1 + ([1] & 1) => TypeError: Unsupported operand types: array & int
--CLEAN--
<?php
?>
