--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `??` fetch of a string offset is quieter than a read, but louder than isset()
--DESCRIPTION--
php has THREE diagnostic levels for a string offset and PHL had two. A `??` fetch
suppresses the NOT-SET diagnostics — no `Uninitialized string offset`, and the
null/bool/float cast notice — but it still WARNS about the offset's SHAPE and
still reads that offset: `$s = "abc"; $s["1x"] ?? "d"` warns
`Illegal string offset "1x"` and answers "b", where PHL called it not-set and
answered "d" — a wrong VALUE, not just a missing warning. It also still RAISES for
an offset TYPE a string refuses (`$s[[]] ?? "d"` is the TypeError), where
isset()/empty() answer false in silence. Riding along: an INTEGRAL float offset
carries a cached int, so `$s[9.0] = "z"` skipped php's `String offset cast
occurred` warning that `$s[9.7] = "z"` emitted — the same cached-int trap as
`~2.0`.
--FILE--
<?php
function socrTry($label, $fn) {
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ": " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}
set_error_handler(function ($no, $msg) { echo "  Warning[$no]: $msg\n"; return true; });

// `??`: an int-then-garbage offset warns and IS read.
socrTry('$s["1x"] ?? "d"', function () { $s = "abc"; return $s["1x"] ?? "d"; });
socrTry('$s["0x1"] ?? "d"', function () { $s = "abc"; return $s["0x1"] ?? "d"; });
socrTry('$s["9x"] ?? "d"', function () { $s = "abc"; return $s["9x"] ?? "d"; });
socrTry('$s["1 x"] ?? "d"', function () { $s = "abc"; return $s["1 x"] ?? "d"; });
// ... while a clean integer string, and the not-set cases, stay silent.
socrTry('$s["1"] ?? "d"', function () { $s = "abc"; return $s["1"] ?? "d"; });
socrTry('$s[" 1 "] ?? "d"', function () { $s = "abc"; return $s[" 1 "] ?? "d"; });
socrTry('$s[9] ?? "d"', function () { $s = "abc"; return $s[9] ?? "d"; });
socrTry('$s[-9] ?? "d"', function () { $s = "abc"; return $s[-9] ?? "d"; });
socrTry('$s["x"] ?? "d"', function () { $s = "abc"; return $s["x"] ?? "d"; });
socrTry('$s[""] ?? "d"', function () { $s = "abc"; return $s[""] ?? "d"; });
// ... and the cast notice is a not-set-class diagnostic: quiet here, loud in a read.
socrTry('$s[1.7] ?? "d"', function () { $s = "abc"; return $s[1.7] ?? "d"; });
socrTry('$s[null] ?? "d"', function () { $s = "abc"; return $s[null] ?? "d"; });
socrTry('$s[true] ?? "d"', function () { $s = "abc"; return $s[true] ?? "d"; });
// An offset TYPE a string refuses still raises in a `??` fetch.
socrTry('$s[[]] ?? "d"', function () { $s = "abc"; return $s[[]] ?? "d"; });
socrTry('$s[$o] ?? "d"', function () { $s = "abc"; $o = new stdClass(); return $s[$o] ?? "d"; });

// isset()/empty() are fully quiet about every one of those shapes.
socrTry('isset($s["1x"])', function () { $s = "abc"; return isset($s["1x"]); });
socrTry('isset($s["0x1"])', function () { $s = "abc"; return isset($s["0x1"]); });
socrTry('isset($s[[]])', function () { $s = "abc"; return isset($s[[]]); });
socrTry('isset($s[$o])', function () { $s = "abc"; $o = new stdClass(); return isset($s[$o]); });
// (an isset() over a LOSSY float offset — isset($s[1.7]) — is left out: php
// emits E_DEPRECATED "Implicit conversion from float" there, and §10 removed
// every engine deprecation site, so only that pair diverges. An integral float
// carries no loss and is silent in both.)
socrTry('isset($s[1.0])', function () { $s = "abc"; return isset($s[1.0]); });
socrTry('isset($s[9])', function () { $s = "abc"; return isset($s[9]); });
socrTry('empty($s["1x"])', function () { $s = "abc"; return empty($s["1x"]); });
socrTry('empty($s[[]])', function () { $s = "abc"; return empty($s[[]]); });

// A real READ is the loud level: shape AND not-set AND cast all report.
socrTry('$s["1x"]', function () { $s = "abc"; return $s["1x"]; });
socrTry('$s["9x"]', function () { $s = "abc"; return $s["9x"]; });
socrTry('$s[9]', function () { $s = "abc"; return $s[9]; });
socrTry('$s[1.7]', function () { $s = "abc"; return $s[1.7]; });
socrTry('$s[null]', function () { $s = "abc"; return $s[null]; });
socrTry('$s["x"]', function () { $s = "abc"; return $s["x"]; });
// ... and so is a WRITE, including for an offset whose float is integral.
socrTry('$s[9.0] = "z"', function () { $s = "abc"; $s[9.0] = "z"; return $s; });
socrTry('$s[9.7] = "z"', function () { $s = "abc"; $s[9.7] = "z"; return $s; });
socrTry('$s[1.0] = "Z"', function () { $s = "abc"; $s[1.0] = "Z"; return $s; });
socrTry('$s[null] = "Q"', function () { $s = "abc"; $s[null] = "Q"; return $s; });
socrTry('$s["1x"] = "Z"', function () { $s = "abc"; $s["1x"] = "Z"; return $s; });
restore_error_handler();
?>
--EXPECT--
  Warning[2]: Illegal string offset "1x"
$s["1x"] ?? "d" => 'b'
  Warning[2]: Illegal string offset "0x1"
$s["0x1"] ?? "d" => 'a'
  Warning[2]: Illegal string offset "9x"
$s["9x"] ?? "d" => 'd'
  Warning[2]: Illegal string offset "1 x"
$s["1 x"] ?? "d" => 'b'
$s["1"] ?? "d" => 'b'
$s[" 1 "] ?? "d" => 'b'
$s[9] ?? "d" => 'd'
$s[-9] ?? "d" => 'd'
$s["x"] ?? "d" => 'd'
$s[""] ?? "d" => 'd'
$s[1.7] ?? "d" => 'b'
$s[null] ?? "d" => 'a'
$s[true] ?? "d" => 'b'
$s[[]] ?? "d" => TypeError: Cannot access offset of type array on string
$s[$o] ?? "d" => TypeError: Cannot access offset of type stdClass on string
isset($s["1x"]) => false
isset($s["0x1"]) => false
isset($s[[]]) => false
isset($s[$o]) => false
isset($s[1.0]) => true
isset($s[9]) => false
empty($s["1x"]) => true
empty($s[[]]) => true
  Warning[2]: Illegal string offset "1x"
$s["1x"] => 'b'
  Warning[2]: Illegal string offset "9x"
  Warning[2]: Uninitialized string offset 9
$s["9x"] => ''
  Warning[2]: Uninitialized string offset 9
$s[9] => ''
  Warning[2]: String offset cast occurred
$s[1.7] => 'b'
  Warning[2]: String offset cast occurred
$s[null] => 'a'
$s["x"] => TypeError: Cannot access offset of type string on string
  Warning[2]: String offset cast occurred
$s[9.0] = "z" => 'abc      z'
  Warning[2]: String offset cast occurred
$s[9.7] = "z" => 'abc      z'
  Warning[2]: String offset cast occurred
$s[1.0] = "Z" => 'aZc'
  Warning[2]: String offset cast occurred
$s[null] = "Q" => 'Qbc'
  Warning[2]: Illegal string offset "1x"
$s["1x"] = "Z" => 'aZc'
--CLEAN--
<?php
?>
