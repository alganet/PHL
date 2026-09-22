--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`~` over a string is a per-byte complement; null/bool/array/object/resource are TypeErrors
--DESCRIPTION--
php's `~` has three arms, and PHL had only one: it cast every operand to an
integer. A STRING is complemented BYTE BY BYTE and stays a string of the same
length (`~"abc"` is "\x9e\x9d\x9c", where PHL answered int(-1)); null, a bool, an
array, an object and a resource have no bitwise not at all
(`TypeError: Cannot perform bitwise not on true`, naming a bool as the literal
`true`/`false` and an object as its class, where PHL answered ~0/~1); and only a
number is complemented as an integer — including an INTEGRAL FLOAT, whose cached
int made `~2.0` answer 2.0 instead of -3.
--FILE--
<?php
function bnocTry($label, $fn) {
    try {
        $r = $fn();
        // Hex in brackets: the expectation must not end in a trailing space
        // for the empty-string case (whitespace an editor could strip).
        $out = is_string($r) ? "string(" . strlen($r) . ") [" . bin2hex($r) . "]" : var_export($r, true);
    } catch (TypeError $e) {
        $out = "TypeError: " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}

class BnocP {}
class BnocS { public function __toString(): string { return "5"; } }

// Strings: byte-wise, length-preserving, binary-safe.
bnocTry('~"abc"', fn() => ~"abc");
bnocTry('~"12"', fn() => ~"12");
bnocTry('~""', fn() => ~"");
bnocTry('~"\xff\x00\x80"', fn() => ~"\xff\x00\x80");
bnocTry('~~"abc" round-trip', fn() => ~~"abc");
bnocTry('~$s[1] (one byte)', function () { $s = "abc"; return ~$s[1]; });
bnocTry('~ of a var', function () { $s = "abc"; $t = ~$s; return [bin2hex($t), $s]; });

// Numbers, including the integral-float case.
bnocTry('~0', fn() => ~0);
bnocTry('~5', fn() => ~5);
bnocTry('~-1', fn() => ~-1);
bnocTry('~2.0', fn() => ~2.0);
bnocTry('~2.0 is an int', fn() => is_int(~2.0));
bnocTry('~-3.0', fn() => ~-3.0);
bnocTry('~PHP_INT_MAX', fn() => ~PHP_INT_MAX);

// Everything else is a TypeError naming the operand.
bnocTry('~null', function () { $n = null; return ~$n; });
bnocTry('~true', fn() => ~true);
bnocTry('~false', fn() => ~false);
bnocTry('~[1]', fn() => ~[1]);
bnocTry('~[]', fn() => ~[]);
bnocTry('~$o', function () { $o = new BnocP(); return ~$o; });
bnocTry('~$str_obj', function () { $o = new BnocS(); return ~$o; });
bnocTry('~$closure', function () { $c = function () {}; return ~$c; });
bnocTry('~$res', function () { $r = fopen("php://memory", "r"); return ~$r; });

// The operand is untouched and execution carries on after the catch.
$bnocA = [1, 2];
try { $x = ~$bnocA; } catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", count($bnocA), " element(s)\n";
// Mid-expression: the throw abandons the rest of the expression.
bnocTry('1 + (~$a)', function () { $a = [1]; return 1 + (~$a); });
bnocTry('"v" . (~$o)', function () { $o = new BnocP(); return "v" . (~$o); });
?>
--EXPECT--
~"abc" => string(3) [9e9d9c]
~"12" => string(2) [cecd]
~"" => string(0) []
~"\xff\x00\x80" => string(3) [00ff7f]
~~"abc" round-trip => string(3) [616263]
~$s[1] (one byte) => string(1) [9d]
~ of a var => array (
  0 => '9e9d9c',
  1 => 'abc',
)
~0 => -1
~5 => -6
~-1 => 0
~2.0 => -3
~2.0 is an int => true
~-3.0 => 2
~PHP_INT_MAX => -9223372036854775807-1
~null => TypeError: Cannot perform bitwise not on null
~true => TypeError: Cannot perform bitwise not on true
~false => TypeError: Cannot perform bitwise not on false
~[1] => TypeError: Cannot perform bitwise not on array
~[] => TypeError: Cannot perform bitwise not on array
~$o => TypeError: Cannot perform bitwise not on BnocP
~$str_obj => TypeError: Cannot perform bitwise not on BnocS
~$closure => TypeError: Cannot perform bitwise not on Closure
~$res => TypeError: Cannot perform bitwise not on resource
caught: Cannot perform bitwise not on array
still 2 element(s)
1 + (~$a) => TypeError: Cannot perform bitwise not on array
"v" . (~$o) => TypeError: Cannot perform bitwise not on BnocP
--CLEAN--
<?php
?>
