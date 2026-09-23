--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayObject/ArrayIterator show php's storage entry, and json_encode joins the hook
--DESCRIPTION--
php has TWO presentation handlers for the array store and they disagree, which is why the hook
is told which is asking. `spl_array_get_debug_info` always shows ONE entry — the storage under
its MANGLED private name, after whatever real properties the instance has — so var_dump and
print_r read `["storage":"ArrayObject":private]`. `spl_array_get_properties_for` answers the
storage's ELEMENTS directly for var_export, the (array) cast and json_encode, with no `storage`
key at all, and hands back the ordinary property table when STD_PROP_LIST is set. The flag is
therefore visible on one surface and invisible on the other.
PHL showed NOTHING on all four, the slots being hidden. Two things fell out of wiring it:
**json_encode never consulted the hook at all**, so `json_encode(new DateTime)` was `{}` where
php prints the three keys; and the hook's callers FELL BACK to the raw slot walk when it filled
nothing, which is wrong for a class that legitimately presents an empty set — an ArrayObject
subclass with an empty storage cast to its own property instead of to php's empty array.
The mangled name always spells the ROOT class: a RecursiveArrayIterator shows `"ArrayIterator"`.
--FILE--
<?php
function apShow($label, $fn) {
    try { $out = $fn(); if (!is_string($out)) { $out = var_export($out, true); } }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace(["\n", "\0"], ['~', '^@'], $out), "\n";
}
// The object ID is engine- and history-dependent (this corpus shares one
// interpreter), so it is stripped rather than pinned.
function apDump($o) { ob_start(); var_dump($o); return preg_replace('/#\d+ /', '', ob_get_clean()); }
class ApKid extends ArrayObject { public $p = 1; }

echo "-- the DEBUG surface is one mangled entry\n";
apShow('ArrayObject', fn() => apDump(new ArrayObject([1, 'k' => 2])));
apShow('ArrayIterator', fn() => apDump(new ArrayIterator(['k' => 2])));
apShow('RecursiveArrayIterator names the ROOT', fn() => apDump(new RecursiveArrayIterator([1])));
apShow('print_r', fn() => print_r(new ArrayObject(['k' => 1]), true));
// The instance's own properties come FIRST, then the storage entry.
apShow('subclass', fn() => apDump(new ApKid([7])));

echo "-- the other surfaces are the ELEMENTS, with no storage key\n";
apShow('(array)', fn() => json_encode((array)new ArrayObject([1, 'k' => 2])));
apShow('var_export', fn() => var_export(new ArrayObject([1, 'k' => 2]), true));
apShow('json_encode', fn() => json_encode(new ArrayObject([1, 'k' => 2])));
// php emits an OBJECT whatever the keys look like: a plain list is not [1,2].
apShow('json of a list', fn() => json_encode(new ArrayObject([1, 2])));
apShow('json nested', fn() => json_encode(['x' => new ArrayIterator(['k' => 1])]));
apShow('subclass (array) drops $p', fn() => json_encode((array)new ApKid([7])));

echo "-- STD_PROP_LIST is visible on one surface and not the other\n";
$apStd = new ArrayObject(['k' => 2], ArrayObject::STD_PROP_LIST);
apShow('var_dump still shows storage', fn() => apDump($apStd));
apShow('but (array) is the property table', fn() => json_encode((array)$apStd));
apShow('and so is json', fn() => json_encode($apStd));

echo "-- an EMPTY presentation is php's answer, not a reason to fall back\n";
$apEmpty = new ApKid;
apShow('(array) of an empty subclass', fn() => json_encode((array)$apEmpty));
apShow('var_export of it', fn() => var_export($apEmpty, true));
apShow('json of it', fn() => json_encode($apEmpty));

echo "-- json_encode now reads the hook for every class that has one\n";
apShow('DateTime', fn() => json_encode(new DateTime('2021-01-01', new DateTimeZone('UTC'))));
apShow('DateTimeZone', fn() => json_encode(new DateTimeZone('UTC')));
apShow('WeakReference shows nothing here', fn() => json_encode(WeakReference::create(new stdClass)));

echo "-- and the surfaces the hook never feeds are unchanged\n";
$apIt = new ArrayObject([1, 'k' => 2]);
apShow('get_object_vars', fn() => json_encode(get_object_vars($apIt)));
apShow('foreach', function () use ($apIt) {
    $out = [];
    foreach ($apIt as $k => $v) { $out[] = "$k=$v"; }
    return implode(',', $out);
});
apShow('count', fn() => (string)count($apIt));
--EXPECT--
-- the DEBUG surface is one mangled entry
ArrayObject => object(ArrayObject)(1) {~  ["storage":"ArrayObject":private]=>~  array(2) {~    [0]=>~    int(1)~    ["k"]=>~    int(2)~  }~}~
ArrayIterator => object(ArrayIterator)(1) {~  ["storage":"ArrayIterator":private]=>~  array(1) {~    ["k"]=>~    int(2)~  }~}~
RecursiveArrayIterator names the ROOT => object(RecursiveArrayIterator)(1) {~  ["storage":"ArrayIterator":private]=>~  array(1) {~    [0]=>~    int(1)~  }~}~
print_r => ArrayObject Object~(~    [storage:ArrayObject:private] => Array~        (~            [k] => 1~        )~~)~
subclass => object(ApKid)(2) {~  ["p"]=>~  int(1)~  ["storage":"ArrayObject":private]=>~  array(1) {~    [0]=>~    int(7)~  }~}~
-- the other surfaces are the ELEMENTS, with no storage key
(array) => {"0":1,"k":2}
var_export => \ArrayObject::__set_state(array(~   0 => 1,~   'k' => 2,~))
json_encode => {"0":1,"k":2}
json of a list => {"0":1,"1":2}
json nested => {"x":{"k":1}}
subclass (array) drops $p => [7]
-- STD_PROP_LIST is visible on one surface and not the other
var_dump still shows storage => object(ArrayObject)(1) {~  ["storage":"ArrayObject":private]=>~  array(1) {~    ["k"]=>~    int(2)~  }~}~
but (array) is the property table => []
and so is json => {}
-- an EMPTY presentation is php's answer, not a reason to fall back
(array) of an empty subclass => []
var_export of it => \ApKid::__set_state(array(~))
json of it => {}
-- json_encode now reads the hook for every class that has one
DateTime => {"date":"2021-01-01 00:00:00.000000","timezone_type":3,"timezone":"UTC"}
DateTimeZone => {"timezone_type":3,"timezone":"UTC"}
WeakReference shows nothing here => {}
-- and the surfaces the hook never feeds are unchanged
get_object_vars => []
foreach => 0=1,k=2
count => 2
