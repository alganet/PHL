--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter()'s $mode decides WHAT the callback is handed: ARRAY_FILTER_USE_KEY the key alone, ARRAY_FILTER_USE_BOTH the value then the key, anything else the value
--FILE--
<?php
// The two selector constants are php's values and are NOT symmetric with their
// names: USE_BOTH is 1, USE_KEY is 2.
var_dump(ARRAY_FILTER_USE_KEY, ARRAY_FILTER_USE_BOTH);

$afm_a = ['keep' => 0, 'drop' => 1];

// USE_KEY: the callback sees the KEY and never the value, so a falsy value
// survives when its key passes.
var_dump(array_filter($afm_a, static fn($afm_k) => $afm_k === 'keep', ARRAY_FILTER_USE_KEY));

// USE_BOTH: value first, key second, in that order.
$afm_seen = [];
var_dump(array_filter($afm_a, static function ($afm_v, $afm_k) use (&$afm_seen) {
    $afm_seen[] = var_export($afm_v, true) . '/' . var_export($afm_k, true);
    return true;
}, ARRAY_FILTER_USE_BOTH));
var_dump($afm_seen);

// Integer keys arrive as ints, not as their decimal spelling.
var_dump(array_filter([5 => 'a', 6 => 'b'], static function ($afm_k) {
    return $afm_k === 6;
}, ARRAY_FILTER_USE_KEY));

// php compares $mode for equality rather than masking it, so every other number
// -- 0, 3, -1, a huge one -- is the default value mode. The selector is read at
// full width: 2^32+1 and 2^32+2 must NOT select USE_BOTH / USE_KEY.
foreach ([0, 3, -1, PHP_INT_MAX, 4294967297, 4294967298] as $afm_mode) {
    var_dump(array_keys(array_filter($afm_a, static fn($afm_v) => $afm_v === 1, $afm_mode)));
}

// The selector is dead without a callback: php's "drop the falsy entries" arm
// never looks at a key, so USE_KEY does not turn it into a key filter.
var_dump(array_filter($afm_a, null, ARRAY_FILTER_USE_KEY));
var_dump(array_filter($afm_a, null, ARRAY_FILTER_USE_BOTH));
var_dump(array_filter($afm_a));

// An ordinary named function reaches the same three shapes.
var_dump(array_filter(['' => 1, 'x' => 2], 'strlen', ARRAY_FILTER_USE_KEY));

// Empty array: the callback is never called and the mode is irrelevant.
var_dump(array_filter([], static fn($afm_k) => true, ARRAY_FILTER_USE_KEY));
?>
--EXPECT--
int(2)
int(1)
array(1) {
  ["keep"]=>
  int(0)
}
array(2) {
  ["keep"]=>
  int(0)
  ["drop"]=>
  int(1)
}
array(2) {
  [0]=>
  string(8) "0/'keep'"
  [1]=>
  string(8) "1/'drop'"
}
array(1) {
  [6]=>
  string(1) "b"
}
array(1) {
  [0]=>
  string(4) "drop"
}
array(1) {
  [0]=>
  string(4) "drop"
}
array(1) {
  [0]=>
  string(4) "drop"
}
array(1) {
  [0]=>
  string(4) "drop"
}
array(1) {
  [0]=>
  string(4) "drop"
}
array(1) {
  [0]=>
  string(4) "drop"
}
array(1) {
  ["drop"]=>
  int(1)
}
array(1) {
  ["drop"]=>
  int(1)
}
array(1) {
  ["drop"]=>
  int(1)
}
array(1) {
  ["x"]=>
  int(2)
}
array(0) {
}
