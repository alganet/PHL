--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A string offset cannot be read-modify-written: ++/-- and every op= are php Errors
--DESCRIPTION--
php refuses to modify a string offset in place: `$s[0]++` is
`Error: Cannot increment/decrement string offsets` and `$s[0] += 1` is
`Error: Cannot use assign-op operators with string offsets`. The value read out
of a string carries the BASE VARIABLE's slot index — a string offset is not a
slot of its own — so PHL wrote the computed result back through that index and
REPLACED the whole string with it: `$s = "5abc"; $s[0] += 1;` left `$s === 6`,
and `$s[0] .= "x"` appended to the whole string. Same marker the
reference-binding sites already test (`$r = &$s[1]`).
--FILE--
<?php
function sormwTry($label, $fn) {
    // Build the line before echoing it, so a diagnostic raised inside $fn
    // cannot land in the middle of it.
    try {
        $out = var_export($fn(), true);
    } catch (Error $e) {
        $out = get_class($e) . ": " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}

// ++/--, both fixities, whatever byte the offset holds.
sormwTry('$s[0]++ (numeric byte)', function () { $s = "5abc"; $s[0]++; return $s; });
sormwTry('$s[0]-- (numeric byte)', function () { $s = "5abc"; $s[0]--; return $s; });
sormwTry('++$s[0]', function () { $s = "5abc"; ++$s[0]; return $s; });
sormwTry('--$s[0]', function () { $s = "5abc"; --$s[0]; return $s; });
sormwTry('$s[0]++ (letter)', function () { $s = "abc"; $s[0]++; return $s; });
sormwTry('$s[-1]++ (negative)', function () { $s = "abc"; $s[-1]++; return $s; });
sormwTry('$s[1]++ (single char)', function () { $s = "5"; $s[0]++; return $s; });

// Every compound assignment operator.
sormwTry('$s[0] += 1', function () { $s = "5abc"; $s[0] += 1; return $s; });
sormwTry('$s[0] -= 1', function () { $s = "5abc"; $s[0] -= 1; return $s; });
sormwTry('$s[0] *= 2', function () { $s = "5abc"; $s[0] *= 2; return $s; });
sormwTry('$s[0] /= 5', function () { $s = "5abc"; $s[0] /= 5; return $s; });
sormwTry('$s[0] %= 2', function () { $s = "5abc"; $s[0] %= 2; return $s; });
sormwTry('$s[0] **= 2', function () { $s = "5abc"; $s[0] **= 2; return $s; });
sormwTry('$s[0] .= "x"', function () { $s = "5abc"; $s[0] .= "x"; return $s; });
sormwTry('$s[0] &= 1', function () { $s = "5abc"; $s[0] &= 1; return $s; });
sormwTry('$s[0] |= 2', function () { $s = "5abc"; $s[0] |= 2; return $s; });
sormwTry('$s[0] ^= 3', function () { $s = "5abc"; $s[0] ^= 3; return $s; });
sormwTry('$s[0] <<= 1', function () { $s = "5abc"; $s[0] <<= 1; return $s; });
sormwTry('$s[0] >>= 1', function () { $s = "5abc"; $s[0] >>= 1; return $s; });

// The string is untouched and execution carries on after the catch.
$sormwS = "5abc";
try { $sormwS[0] += 1; } catch (Error $e) { echo "caught: ", $e->getMessage(), "\n"; }
try { $sormwS[1]++; } catch (Error $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", var_export($sormwS, true), "\n";
// Mid-expression: the throw abandons the rest of the expression.
sormwTry('1 + ($s[0] += 1)', function () { $s = "5abc"; return 1 + ($s[0] += 1); });

// What still works: a plain offset store, and the same operators on an ARRAY
// element or a whole string variable.
sormwTry('$s[0] = "Z"', function () { $s = "5abc"; $s[0] = "Z"; return $s; });
sormwTry('$s .= "d"', function () { $s = "5abc"; $s .= "d"; return $s; });
sormwTry('$a[0] += 1', function () { $a = [5]; $a[0] += 1; return $a[0]; });
sormwTry('$a[0]++', function () { $a = [5]; $a[0]++; return $a[0]; });
sormwTry('$a["k"] .= "x"', function () { $a = ["k" => "v"]; $a["k"] .= "x"; return $a["k"]; });
sormwTry('$copy = $s[0]; $copy++', function () { $s = "5abc"; $copy = $s[0]; $copy++; return [$copy, $s]; });
?>
--EXPECT--
$s[0]++ (numeric byte) => Error: Cannot increment/decrement string offsets
$s[0]-- (numeric byte) => Error: Cannot increment/decrement string offsets
++$s[0] => Error: Cannot increment/decrement string offsets
--$s[0] => Error: Cannot increment/decrement string offsets
$s[0]++ (letter) => Error: Cannot increment/decrement string offsets
$s[-1]++ (negative) => Error: Cannot increment/decrement string offsets
$s[1]++ (single char) => Error: Cannot increment/decrement string offsets
$s[0] += 1 => Error: Cannot use assign-op operators with string offsets
$s[0] -= 1 => Error: Cannot use assign-op operators with string offsets
$s[0] *= 2 => Error: Cannot use assign-op operators with string offsets
$s[0] /= 5 => Error: Cannot use assign-op operators with string offsets
$s[0] %= 2 => Error: Cannot use assign-op operators with string offsets
$s[0] **= 2 => Error: Cannot use assign-op operators with string offsets
$s[0] .= "x" => Error: Cannot use assign-op operators with string offsets
$s[0] &= 1 => Error: Cannot use assign-op operators with string offsets
$s[0] |= 2 => Error: Cannot use assign-op operators with string offsets
$s[0] ^= 3 => Error: Cannot use assign-op operators with string offsets
$s[0] <<= 1 => Error: Cannot use assign-op operators with string offsets
$s[0] >>= 1 => Error: Cannot use assign-op operators with string offsets
caught: Cannot use assign-op operators with string offsets
caught: Cannot increment/decrement string offsets
still '5abc'
1 + ($s[0] += 1) => Error: Cannot use assign-op operators with string offsets
$s[0] = "Z" => 'Zabc'
$s .= "d" => '5abcd'
$a[0] += 1 => 6
$a[0]++ => 6
$a["k"] .= "x" => 'vx'
$copy = $s[0]; $copy++ => array (
  0 => 6,
  1 => '5abc',
)
--CLEAN--
<?php
?>
