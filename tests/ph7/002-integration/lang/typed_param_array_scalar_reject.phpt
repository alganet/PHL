--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed array/scalar parameter rejects the other kind with TypeError, never coerces
--FILE--
<?php
// php never weak-coerces between array and scalar for a type declaration: a
// scalar passed to an `array` parameter, or an array passed to a scalar
// parameter, is a TypeError — not a silent [scalar] wrap or an "Array"/int(1)
// truncation. This exercises the positional, generator, and variadic binding
// paths, which all funnel scalar coercion through the same engine helper.
function shortMsg(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    return $msg;
}

// --- scalar (and object) given to an `array` parameter ---
function wantsArray(array $a) { var_dump($a); }
try { wantsArray(5);            } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsArray("x");          } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsArray(true);         } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsArray(1.5);          } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsArray(new stdClass); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- array given to a scalar parameter ---
function wantsInt(int $x)       { var_dump($x); }
function wantsString(string $x) { var_dump($x); }
function wantsBool(bool $x)     { var_dump($x); }
try { wantsInt([1,2]);    } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsString([1]);   } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsBool([1]);     } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- generator parameter binding ---
function genArray(array $a) { yield $a; }
try { $g = genArray(5); $g->current(); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- variadic collecting an `array`: each element is type-checked. Both
// engines throw; php omits the ($name) and numbers by element position in the
// message, so normalize to a marker rather than assert the exact text. ---
function variadicArray(array ...$rows) { return $rows; }
try { variadicArray([1], 2); echo "variadic: no throw\n"; }
catch (TypeError $e) { echo "variadic: rejected\n"; }

// A valid array argument still binds normally.
wantsArray([1, 2]);
?>
--EXPECT--
wantsArray(): Argument #1 ($a) must be of type array, int given
wantsArray(): Argument #1 ($a) must be of type array, string given
wantsArray(): Argument #1 ($a) must be of type array, true given
wantsArray(): Argument #1 ($a) must be of type array, float given
wantsArray(): Argument #1 ($a) must be of type array, stdClass given
wantsInt(): Argument #1 ($x) must be of type int, array given
wantsString(): Argument #1 ($x) must be of type string, array given
wantsBool(): Argument #1 ($x) must be of type bool, array given
genArray(): Argument #1 ($a) must be of type array, int given
variadic: rejected
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
--CLEAN--
<?php
