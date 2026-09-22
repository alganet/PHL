--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`$s[k] ??= v` writes the string OFFSET (and an empty string is refused)
--DESCRIPTION--
`??=` is not an assign-op, so php performs a real string-offset store: `$s = "abc";
$s[9] ??= "z";` pads and writes the byte (`"abc      z"`). PHL wrote the RHS through
the only slot index a string offset carries — the BASE VARIABLE's — and so
REPLACED the whole string with it, leaving `$s === "z"`; `$s["x"] ??= "z"` even
reported the engine's non-catchable "Cannot perform assignment on a constant class
attribute". The offset is consumed by the peek, so it now rides the coalesce rail
to the store, where php re-resolves it LOUDLY (the peek is the quiet half of the
pair). The store also gains php's `Cannot assign an empty string to a string
offset` Error, which the plain `$s[2] = ""` form was silently ignoring.
--FILE--
<?php
function socaTry($label, $fn) {
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ": " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}
set_error_handler(function ($no, $msg) { echo "  Warning[$no]: $msg\n"; return true; });

// Past the end: pad with spaces, then write the first byte.
socaTry('$s[9] ??= "z"', function () { $s = "abc"; $s[9] ??= "z"; return $s; });
socaTry('$s[3] ??= "z"', function () { $s = "abc"; $s[3] ??= "z"; return $s; });
socaTry('$s[3] ??= "zy"', function () { $s = "abc"; $s[3] ??= "zy"; return $s; });
socaTry('$s[3] ??= 5', function () { $s = "abc"; $s[3] ??= 5; return $s; });
socaTry('$e = ""; $e[0] ??= "z"', function () { $e = ""; $e[0] ??= "z"; return $e; });
socaTry('$e = ""; $e[2] ??= "z"', function () { $e = ""; $e[2] ??= "z"; return $e; });
// In range: the byte is set, so the assignment short-circuits.
socaTry('$s[1] ??= "z"', function () { $s = "abc"; $s[1] ??= "z"; return $s; });
socaTry('$s[0] ??= "z" ("0abc")', function () { $s = "0abc"; $s[0] ??= "z"; return $s; });
socaTry('$s[-1] ??= "z"', function () { $s = "abc"; $s[-1] ??= "z"; return $s; });
// Out of range on the negative side: php warns and writes nothing.
socaTry('$s[-9] ??= "z"', function () { $s = "abc"; $s[-9] ??= "z"; return $s; });

// The expression's value is the RHS on a write, the byte on a short-circuit.
socaTry('($s[9] ??= "z") value', function () { $s = "abc"; $r = ($s[9] ??= "z"); return [$r, $s]; });
socaTry('($s[1] ??= "z") value', function () { $s = "abc"; $r = ($s[1] ??= "z"); return [$r, $s]; });

// The offset is re-resolved at the STORE, so its diagnostics land there too.
socaTry('$s["9"] ??= "z"', function () { $s = "abc"; $s["9"] ??= "z"; return $s; });
socaTry('$s["9x"] ??= "z"', function () { $s = "abc"; $s["9x"] ??= "z"; return $s; });
socaTry('$s[9.0] ??= "z"', function () { $s = "abc"; $s[9.0] ??= "z"; return $s; });
socaTry('$s[null] ??= "z" ("")', function () { $e = ""; $e[null] ??= "z"; return $e; });
socaTry('$s["x"] ??= "z"', function () { $s = "abc"; $s["x"] ??= "z"; return $s; });
socaTry('$s[[]] ??= "z"', function () { $s = "abc"; $s[[]] ??= "z"; return $s; });
// The RHS takes the ordinary user-visible string coercion.
socaTry('$s[9] ??= []', function () { $s = "abc"; $s[9] ??= []; return $s; });

// An EMPTY replacement is refused — for `??=` and for a plain store alike.
socaTry('$s[9] ??= ""', function () { $s = "abc"; $s[9] ??= ""; return $s; });
socaTry('$s[9] ??= null', function () { $s = "abc"; $s[9] ??= null; return $s; });
socaTry('$s[2] = ""', function () { $s = "abc"; $s[2] = ""; return $s; });
socaTry('$s[2] = null', function () { $s = "abc"; $s[2] = null; return $s; });
socaTry('$s[2] = false', function () { $s = "abc"; $s[2] = false; return $s; });
socaTry('$s[-9] = ""', function () { $s = "abc"; $s[-9] = ""; return $s; });
socaTry('$s[2] = "Z" still works', function () { $s = "abc"; $s[2] = "Z"; return $s; });
$socaS = "abc";
try { $socaS[1] = ""; } catch (Error $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", var_export($socaS, true), "\n";

// The offset is resolved against the string as it is AT THE STORE, so an RHS that
// reassigns the base writes into the new value (php does the same).
socaTry('RHS reassigns the base', function () { $s = "abc"; $s[9] ??= ($s = "QQ") ? "y" : "n"; return $s; });
// A nested ??= inside the RHS must not consume the outer one's pending offset —
// which is why the offset rides the peek's own stack slot rather than one VM-wide
// register: with a register, the inner peek overwrote the outer's offset and the
// outer store replaced the whole string.
socaTry('nested ??=', function () { $s = "abc"; $a = []; $a["k"] ??= ($s[9] ??= "z"); return [$a, $s]; });
socaTry('string ??= inside string ??=', function () { $s = "abc"; $t = "xyz"; $s[9] ??= ($t[9] ??= "q"); return [$s, $t]; });
// An ARRAY ??= right after a string one must not see the string's arm.
socaTry('array ??= after string', function () { $s = "abc"; $s[9] ??= "z"; $a = []; $a["k"] ??= "v"; return [$s, $a]; });
restore_error_handler();
?>
--EXPECT--
$s[9] ??= "z" => 'abc      z'
$s[3] ??= "z" => 'abcz'
  Warning[2]: Only the first byte will be assigned to the string offset
$s[3] ??= "zy" => 'abcz'
$s[3] ??= 5 => 'abc5'
$e = ""; $e[0] ??= "z" => 'z'
$e = ""; $e[2] ??= "z" => '  z'
$s[1] ??= "z" => 'abc'
$s[0] ??= "z" ("0abc") => '0abc'
$s[-1] ??= "z" => 'abc'
  Warning[2]: Illegal string offset -9
$s[-9] ??= "z" => 'abc'
($s[9] ??= "z") value => array (
  0 => 'z',
  1 => 'abc      z',
)
($s[1] ??= "z") value => array (
  0 => 'b',
  1 => 'abc',
)
$s["9"] ??= "z" => 'abc      z'
  Warning[2]: Illegal string offset "9x"
  Warning[2]: Illegal string offset "9x"
$s["9x"] ??= "z" => 'abc      z'
  Warning[2]: String offset cast occurred
$s[9.0] ??= "z" => 'abc      z'
  Warning[2]: String offset cast occurred
$s[null] ??= "z" ("") => 'z'
$s["x"] ??= "z" => TypeError: Cannot access offset of type string on string
$s[[]] ??= "z" => TypeError: Cannot access offset of type array on string
  Warning[2]: Array to string conversion
  Warning[2]: Only the first byte will be assigned to the string offset
$s[9] ??= [] => 'abc      A'
$s[9] ??= "" => Error: Cannot assign an empty string to a string offset
$s[9] ??= null => Error: Cannot assign an empty string to a string offset
$s[2] = "" => Error: Cannot assign an empty string to a string offset
$s[2] = null => Error: Cannot assign an empty string to a string offset
$s[2] = false => Error: Cannot assign an empty string to a string offset
  Warning[2]: Illegal string offset -9
$s[-9] = "" => 'abc'
$s[2] = "Z" still works => 'abZ'
caught: Cannot assign an empty string to a string offset
still 'abc'
RHS reassigns the base => 'QQ       y'
nested ??= => array (
  0 => 
  array (
    'k' => 'z',
  ),
  1 => 'abc      z',
)
string ??= inside string ??= => array (
  0 => 'abc      q',
  1 => 'xyz      q',
)
array ??= after string => array (
  0 => 'abc      z',
  1 => 
  array (
    'k' => 'v',
  ),
)
--CLEAN--
<?php
?>
