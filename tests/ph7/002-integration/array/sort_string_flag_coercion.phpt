--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The STRING sort flags and array_unique() coerce user-visibly: an object with no __toString() throws php's Error (it used to sort on the literal "Object"), and the array still comes out sorted
--DESCRIPTION--
`sort($a, SORT_STRING)` and friends render their operands with php's
zval_get_string(). An ARRAY element warns "Array to string conversion"; an object
with no __toString() raises the catchable "could not be converted to string"
Error and renders as the EMPTY string — which is why php's array comes out fully
SORTED after the throw, with the object first. PHL rendered it as the literal
"Object" and sorted on that, silently. array_unique() is the same site: its
DEFAULT flag is SORT_STRING, so `array_unique([new Bare(), new Bare()])` throws
in php and answered a one-element array in PHL.

A comparator has no status channel, so the Error is raised once per sort and
flagged on the VM through the same iCmpCallbackExc rail a throwing user callback
uses; each flag-sort driver clears it before its merge sort and answers the
exception after, and array_unique() does the same around its walk. Clearing
matters as much as reporting: a leaked flag makes the NEXT comparison-based call
consider every pair equal.

SORT_REGULAR is a different rule and unaffected — it uses the engine comparison,
where an object is simply greater than a string (see
002-integration/oo/object_scalar_comparison.phpt).
--FILE--
<?php
class Bare {}
class Str { public function __toString(): string { return "S"; } }

function t(string $label, callable $f): void {
    try {
        echo $label, " => ", json_encode($f()), "\n";
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
function names(array $a): array {
    return array_map(fn ($v) => is_object($v) ? 'OBJ' : $v, $a);
}

// --- SORT_REGULAR keeps the engine comparison: no throw.
t('sort-regular',  function () { $a = [new Bare(), "b"]; sort($a); return names($a); });
t('unique-regular',function () { return names(array_unique([new Bare(), new Bare()], SORT_REGULAR)); });

// --- the STRING flags throw...
t('sort-string',   function () { $a = [new Bare(), "b"]; sort($a, SORT_STRING); return names($a); });
t('rsort-string',  function () { $a = [new Bare(), "b"]; rsort($a, SORT_STRING); return names($a); });
t('asort-string',  function () { $a = [new Bare(), "b"]; asort($a, SORT_STRING); return names($a); });
t('arsort-string', function () { $a = [new Bare(), "b"]; arsort($a, SORT_STRING); return names($a); });
t('sort-natural',  function () { $a = [new Bare(), "b"]; sort($a, SORT_NATURAL); return names($a); });
t('sort-flagcase', function () { $a = [new Bare(), "b"]; sort($a, SORT_STRING | SORT_FLAG_CASE); return names($a); });
t('sort-locale',   function () { $a = [new Bare(), "b"]; sort($a, SORT_LOCALE_STRING); return names($a); });
t('unique-default',function () { return names(array_unique([new Bare(), new Bare()])); });
t('unique-string', function () { return names(array_unique([new Bare(), "x"], SORT_STRING)); });

// ...and a Stringable still sorts on its string.
t('sort-strable',  function () { $a = [new Str(), "b", "T"]; sort($a, SORT_STRING); return names($a); });
t('unique-strable',function () { return names(array_unique([new Str(), new Str(), "S"])); });

// --- the array STILL comes out sorted after the throw (the object renders as "",
// so it lands first), and the keys behave per function.
$a = ["c", new Bare(), "a"];
try { sort($a, SORT_STRING); } catch (Error $e) { echo "sorted-after-throw: "; }
echo json_encode(names($a)), "\n";
$b = ["c", new Bare(), "a"];
try { asort($b, SORT_STRING); } catch (Error $e) { echo "asort-after-throw: "; }
echo json_encode(names($b)), " keys=", json_encode(array_keys($b)), "\n";

// --- an ARRAY element only warns, and both operands of a comparison convert.
t('sort-array-elem',   function () { $a = [[1], "b"]; sort($a, SORT_STRING); return $a; });
t('unique-array-elems',function () { return array_unique([[1], [1]]); });

// --- a caught throw must not leak the comparator flag into the next call: the
// following sorts and array_unique()s have to behave normally.
try { array_unique([new Bare(), new Bare()]); } catch (Throwable $e) { echo "leak-guard caught\n"; }
echo json_encode(array_unique(["a", "a", "b"])), "\n";
echo json_encode(array_unique([1, "1", 2])), "\n";
$c = ["c", "a", "b"];
sort($c, SORT_STRING);
echo json_encode($c), "\n";
try { $d = ["c", new Bare()]; sort($d, SORT_STRING); } catch (Throwable $e) { echo "leak-guard2 caught\n"; }
$e2 = ["c", "a", "b"];
sort($e2, SORT_STRING);
echo json_encode($e2), "\n";
$f = [3, 1, 2];
usort($f, fn ($x, $y) => $x <=> $y);
echo json_encode($f), "\n";

// --- ksort/krsort take the same flags but their keys are only ever int|string.
t('ksort-string',  function () { $a = ["b" => 1, "a" => new Bare()]; ksort($a, SORT_STRING); return array_keys($a); });
t('krsort-string', function () { $a = ["b" => 1, "a" => new Bare()]; krsort($a, SORT_STRING); return array_keys($a); });
?>
--EXPECTF--
sort-regular => ["b","OBJ"]
unique-regular => ["OBJ"]
sort-string => sort-string => Error: Object of class Bare could not be converted to string
rsort-string => rsort-string => Error: Object of class Bare could not be converted to string
asort-string => asort-string => Error: Object of class Bare could not be converted to string
arsort-string => arsort-string => Error: Object of class Bare could not be converted to string
sort-natural => sort-natural => Error: Object of class Bare could not be converted to string
sort-flagcase => sort-flagcase => Error: Object of class Bare could not be converted to string
sort-locale => sort-locale => Error: Object of class Bare could not be converted to string
unique-default => unique-default => Error: Object of class Bare could not be converted to string
unique-string => unique-string => Error: Object of class Bare could not be converted to string
sort-strable => ["OBJ","T","b"]
unique-strable => ["OBJ"]
sorted-after-throw: ["OBJ","a","c"]
asort-after-throw: {"1":"OBJ","2":"a","0":"c"} keys=[1,2,0]
sort-array-elem => PHP Warning:  Array to string conversion in %s on line %d
[[1],"b"]
unique-array-elems => PHP Warning:  Array to string conversion in %s on line %d
PHP Warning:  Array to string conversion in %s on line %d
[[1]]
leak-guard caught
{"0":"a","2":"b"}
{"0":1,"2":2}
["a","b","c"]
leak-guard2 caught
["a","b","c"]
[1,2,3]
ksort-string => ["a","b"]
krsort-string => ["b","a"]
