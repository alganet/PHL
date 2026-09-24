--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_constants() answers a name => VALUE map, and $categorize groups it one level deeper
--FILE--
<?php
// php's answer is a MAP -- the name is the KEY and the value is the element --
// which is what makes get_defined_constants()['PHP_EOL'] the documented way to
// read one. Answering a LIST of names instead gave the array the right length
// and the wrong shape: every such lookup was an "Undefined array key" and NULL.
$gdc = get_defined_constants();
var_dump($gdc['PHP_EOL'] === PHP_EOL);
var_dump($gdc['PHP_INT_MAX'] === PHP_INT_MAX);
var_dump($gdc['E_ALL'] === E_ALL);
var_dump($gdc['SORT_STRING'] === SORT_STRING);
var_dump($gdc['ARRAY_FILTER_USE_KEY'] === ARRAY_FILTER_USE_KEY);
var_dump($gdc['DATE_RFC7231'] === DATE_RFC7231);
var_dump($gdc['FILE_TEXT'] === FILE_TEXT);
// The keys are the NAMES, so isset() is the membership test (a list of names
// would make in_array() the one that worked and isset() the one that did not).
var_dump(isset($gdc['M_PI']), isset($gdc['NoSuchConstantAnywhere']));
var_dump(is_string(array_key_first($gdc)));

// A constant defined by the script is in there with its value, whatever type.
define('Gdc_int', 42);
define('Gdc_arr', [1, 2]);
define('Gdc_null', null);
const GDC_CONST = 'k';
$gdc = get_defined_constants();
var_dump($gdc['Gdc_int'], $gdc['Gdc_arr'], $gdc['Gdc_null'], $gdc['GDC_CONST']);
// A CLASS constant is not a defined constant.
class GdcHolder { const K = 5; }
$gdc = get_defined_constants();
var_dump(isset($gdc['GdcHolder::K']), isset($gdc['K']));

// $categorize groups one level deeper. The `user` bucket is the one every engine
// names the same way; how the ENGINE's own constants are split is php's extension
// partition, which PHL does not have -- that half is the twin pair in
// 002-integration/constants/get_defined_constants_categorize{,_zend}.phpt.
// The ORDER inside a bucket is each engine's registration order and is not
// asserted (php keeps definition order; PHL walks its constant hash).
$gdc = get_defined_constants(true);
$gdc = array_filter($gdc['user'], static fn($gdc_k) => str_starts_with($gdc_k, 'Gdc'), ARRAY_FILTER_USE_KEY);
ksort($gdc);
var_dump($gdc);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
int(42)
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
NULL
string(1) "k"
bool(false)
bool(false)
array(3) {
  ["Gdc_arr"]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
  ["Gdc_int"]=>
  int(42)
  ["Gdc_null"]=>
  NULL
}
--CLEAN--
<?php
unset($gdc);
