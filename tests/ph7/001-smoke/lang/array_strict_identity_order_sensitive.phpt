--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: array === is order-sensitive (== stays order-insensitive)
--FILE--
<?php
// PHP's '===' on arrays requires the same key/value pairs in the SAME order;
// PHL's hashmap comparison used to match keys by lookup for both === and ==,
// making === wrongly order-insensitive.
$aiA = ['a' => 1, 'b' => 2];
$aiB = ['b' => 2, 'a' => 1];
echo var_export($aiA === $aiB, true), " ", var_export($aiA == $aiB, true), "\n";
// identical order is identical
echo var_export(['x'=>1,'y'=>2] === ['x'=>1,'y'=>2], true), "\n";
// nested reorder is not identical
echo var_export(['p'=>['q'=>1,'r'=>2]] === ['p'=>['r'=>2,'q'=>1]], true), "\n";
// int keys reordered
echo var_export([0=>1,1=>2] === [1=>2,0=>1], true), "\n";
// a numeric-string key normalizes to int, so these ARE identical
echo var_export([1=>'a'] === ['1'=>'a'], true), "\n";
// value type strictness under ===
echo var_export(['k'=>1] === ['k'=>'1'], true), "\n";
// in_array / array_search strict membership stays order-independent (=== per element)
$aiHay = ['1', 1, true];
echo var_export(in_array(1, $aiHay, true), true), " ", var_export(array_search('1', $aiHay, true), true), "\n";
// match() compares with === : a reordered subject only hits an identical arm
$aiSubj = ['b'=>2, 'a'=>1];
echo match($aiSubj) {
    ['a'=>1, 'b'=>2] => "ordered",
    ['b'=>2, 'a'=>1] => "asis",
    default => "none",
}, "\n";
?>
--EXPECT--
false true
true
false
false
true
false
true 0
asis
