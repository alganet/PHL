--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff()/array_intersect() and their _assoc/_uassoc variants compare elements as (string)$a === (string)$b, not with the engine's strict comparison
--DESCRIPTION--
php's manual defines these four as string comparisons: two elements are equal
iff `(string) $elem1 === (string) $elem2`. PHL called HashmapFindValue with
bStrict, so no int ever matched its own decimal string and
`array_diff([1,2,3], ["1","2"])` answered the WHOLE first array instead of
`[2 => 3]` — the common shape being ids diffed against strings out of a form, a
DB row or explode(). The intersects answered `[]` for the same reason.

The comparison is a PURE string compare, NOT numeric-string aware:
`array_diff(["10"], ["1e1"])` keeps "10", while `array_diff([1.10], ["1.1"])` is
empty because (string)1.10 IS "1.1".

Where the coercion DIAGNOSES also follows php's algorithm. array_diff and
array_intersect sort every input array, converting each element exactly once, so
their warnings are per-element and a not-stringable object throws even when an
earlier element already matched. The _assoc and _uassoc variants convert LAZILY,
only for a key that matched somewhere, so an object under a key nobody else has
never throws.
--FILE--
<?php
function t(string $label, callable $f): void {
    try {
        echo $label, " => ", json_encode($f()), "\n";
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

// --- the headline case: int vs its decimal string.
t('diff-num',        fn () => array_diff([1, 2, 3], ["1", "2"]));
t('diff-num-multi',  fn () => array_diff([1, 2, 3], ["1"], ["2"]));
t('isect-num',       fn () => array_intersect([1, 2, 3], ["1", "2"]));
t('isect-multi',     fn () => array_intersect([1, 2, 3], ["1", "2"], ["2", "3"]));
t('diff-dupes',      fn () => array_diff([1, 1, 2], ["1"]));
t('diff-keys-kept',  fn () => array_diff(["x" => 1, "y" => 2], ["1"]));
t('diff-range',      fn () => array_diff(range(1, 10), ["2", "4", "6"]));

// --- every scalar renders through (string).
t('diff-float',      fn () => array_diff([1.0, 1.5], ["1", "1.5"]));
t('diff-trailing0',  fn () => array_diff([1.10], ["1.1"]));
t('diff-bignum',     fn () => array_diff([1e20], ["1.0E+20"]));
t('diff-neg0',       fn () => array_diff([-0.0], ["-0"]));
t('diff-bool',       fn () => array_diff([true, false], ["1", ""]));
t('diff-null',       fn () => array_diff([null], [""]));

// --- and it is NOT numeric-string aware.
t('diff-1e1',        fn () => array_diff(["10"], ["1e1"]));
t('diff-int-1e1',    fn () => array_diff([10], ["1e1"]));
t('diff-space',      fn () => array_diff(["1"], [" 1"]));
t('isect-1e1',       fn () => array_intersect([10], ["1e1"]));

// --- empty operands.
t('diff-empty-2nd',  fn () => array_diff([1], []));
t('diff-empty-1st',  fn () => array_diff([], [1]));

// --- the _assoc pair compares keys AND the string form of the values.
t('assoc-num',       fn () => array_diff_assoc(["a" => 1], ["a" => "1"]));
t('assoc-key-only',  fn () => array_diff_assoc(["a" => 1], ["b" => 1]));
t('assoc-intkey',    fn () => array_diff_assoc([0 => 1], ["0" => "1"]));
t('assoc-partial',   fn () => array_diff_assoc(["a" => 1, "b" => 2], ["a" => "1"]));
t('assoc-mixed',     fn () => array_diff_assoc(["x" => 1, "y" => "2", 3 => 3.0], ["x" => "1", "y" => 2, 3 => "3"]));
t('iassoc-num',      fn () => array_intersect_assoc(["a" => 1], ["a" => "1"]));
t('iassoc-partial',  fn () => array_intersect_assoc(["a" => 1, "b" => 2], ["a" => "1", "b" => "3"]));
t('iassoc-mixed',    fn () => array_intersect_assoc(["x" => 1, "y" => "2", 3 => 3.0], ["x" => "1", "y" => 2, 3 => "3"]));

// --- _uassoc puts the KEYS through the callback and leaves the VALUES on the
// string comparison.
t('diff-uassoc',     fn () => array_diff_uassoc(["a" => 1], ["a" => "1"], fn ($x, $y) => strcmp($x, $y)));
t('diff-uassoc-key', fn () => array_diff_uassoc(["a" => 1], ["A" => "1"], fn ($x, $y) => strcasecmp($x, $y)));
t('diff-uassoc-val', fn () => array_diff_uassoc(["a" => 1], ["a" => "2"], fn ($x, $y) => strcmp($x, $y)));

// --- the callback and key-only variants keep their own rules.
t('udiff',           fn () => array_udiff([1, 2, 3], ["1"], fn ($a, $b) => $a <=> $b));
t('uintersect',      fn () => array_uintersect([1, 2, 3], ["1", "2"], fn ($a, $b) => $a <=> $b));
t('diff-key',        fn () => array_diff_key(["a" => 1, "b" => 2], ["a" => 9]));
t('isect-key',       fn () => array_intersect_key(["a" => 1, "b" => 2], ["a" => 9]));

// --- so do the ENGINE-comparison searches, which are a different rule.
t('in_array-loose',  fn () => in_array("1", [1]));
t('in_array-strict', fn () => in_array("1", [1], true));
t('array_search',    fn () => array_search("1", [1]));
t('array_keys',      fn () => array_keys([1, "1"], "1"));
t('array_unique',    fn () => array_unique([1, "1"]));

// --- an ARRAY element warns once per element, as php's sort converts it once.
t('arr-elem-1',      fn () => array_diff([[1]], ["Array"]));
t('arr-elem-2',      fn () => array_diff([[1], [2]], ["Array"]));
t('arr-elem-rhs',    fn () => array_diff([1, 2, 3], [[9]]));
t('arr-elem-nested', fn () => array_diff([[1]], [[2]]));
t('assoc-arr-both',  fn () => array_diff_assoc(["a" => [1]], ["a" => [1]]));
t('iassoc-arr',      fn () => array_intersect_assoc(["a" => [1]], ["a" => "Array"]));

// --- an object with no __toString() throws php's coercion Error. array_diff
// converts up front, so it throws even though the first element MATCHED; the
// _assoc pair converts lazily, so an unmatched key never coerces.
class Bare {}
class Str { public function __toString(): string { return "1"; } }
t('diff-obj-lhs',    fn () => array_diff([new Bare()], ["x"]));
t('diff-obj-rhs',    fn () => array_diff(["x"], [new Bare()]));
t('diff-obj-after-match', fn () => array_diff([1, 2], [1, new Bare()]));
t('isect-obj',       fn () => array_intersect([1], [new Bare()]));
t('diff-strable',    fn () => array_diff([new Str()], ["1"]));
t('isect-strable',   fn () => array_intersect([new Str()], ["1"]));
t('assoc-obj-match', fn () => array_diff_assoc(["a" => 1], ["a" => new Bare()]));
t('assoc-obj-nokey', fn () => array_diff_assoc(["a" => 1], ["b" => new Bare()]));

// --- one argument: php sorts anyway in array_diff (so it warns) and skips the
// sort in array_intersect (so it does not). Asymmetric, and pinned as such.
t('diff-1arg',       fn () => array_diff([[1]]));
t('isect-1arg',      fn () => array_intersect([[1]]));
?>
--EXPECTF--
diff-num => {"2":3}
diff-num-multi => {"2":3}
isect-num => [1,2]
isect-multi => {"1":2}
diff-dupes => {"2":2}
diff-keys-kept => {"y":2}
diff-range => {"0":1,"2":3,"4":5,"6":7,"7":8,"8":9,"9":10}
diff-float => []
diff-trailing0 => []
diff-bignum => []
diff-neg0 => []
diff-bool => []
diff-null => []
diff-1e1 => ["10"]
diff-int-1e1 => [10]
diff-space => ["1"]
isect-1e1 => []
diff-empty-2nd => [1]
diff-empty-1st => []
assoc-num => []
assoc-key-only => {"a":1}
assoc-intkey => []
assoc-partial => {"b":2}
assoc-mixed => []
iassoc-num => {"a":1}
iassoc-partial => {"a":1}
iassoc-mixed => {"x":1,"y":"2","3":3}
diff-uassoc => []
diff-uassoc-key => []
diff-uassoc-val => {"a":1}
udiff => {"1":2,"2":3}
uintersect => [1,2]
diff-key => {"b":2}
isect-key => {"a":1}
in_array-loose => true
in_array-strict => false
array_search => 0
array_keys => [0,1]
array_unique => [1]
arr-elem-1 => PHP Warning:  Array to string conversion in %s on line %d
[]
arr-elem-2 => PHP Warning:  Array to string conversion in %s on line %d
PHP Warning:  Array to string conversion in %s on line %d
[]
arr-elem-rhs => PHP Warning:  Array to string conversion in %s on line %d
[1,2,3]
arr-elem-nested => PHP Warning:  Array to string conversion in %s on line %d
PHP Warning:  Array to string conversion in %s on line %d
[]
assoc-arr-both => PHP Warning:  Array to string conversion in %s on line %d
PHP Warning:  Array to string conversion in %s on line %d
[]
iassoc-arr => PHP Warning:  Array to string conversion in %s on line %d
{"a":[1]}
diff-obj-lhs => diff-obj-lhs => Error: Object of class Bare could not be converted to string
diff-obj-rhs => diff-obj-rhs => Error: Object of class Bare could not be converted to string
diff-obj-after-match => diff-obj-after-match => Error: Object of class Bare could not be converted to string
isect-obj => isect-obj => Error: Object of class Bare could not be converted to string
diff-strable => []
isect-strable => [{}]
assoc-obj-match => assoc-obj-match => Error: Object of class Bare could not be converted to string
assoc-obj-nokey => {"a":1}
diff-1arg => PHP Warning:  Array to string conversion in %s on line %d
[[1]]
isect-1arg => [[1]]
