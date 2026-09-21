--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE: a lossy float/float-string int argument is a TypeError that ABORTS the call — no array is produced. php's deprecating half lives in the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// php narrows the fractional value with a deprecation and still builds the array.
// §10 rejects the lossy narrowing loudly, so the throw stops the builtin: $alia_r
// keeps its previous value and the echo after the call never runs.
$alia_r = "untouched";
foreach ([
    'fill_count'     => static fn() => array_fill(1, 1.5, "v"),
    'fill_start_str' => static fn() => array_fill("1.5", 2, "v"),
    'chunk_len'      => static fn() => array_chunk([1, 2, 3], 1.5),
    'chunk_len_str'  => static fn() => array_chunk([1, 2, 3], "1.5"),
] as $alia_name => $alia_fn) {
    try {
        $alia_r = $alia_fn();
        echo "NO_THROW\n";
    } catch (\TypeError $e) {
        echo $alia_name, ": ", $e->getMessage(), "\n";
    }
}
var_dump($alia_r);
// a WHOLE float is not lossy and still works
var_dump(array_fill(0, 2.0, "w"));
?>
--EXPECT--
fill_count: Implicit conversion from float to int loses precision
fill_start_str: Implicit conversion from float-string to int loses precision
chunk_len: Implicit conversion from float to int loses precision
chunk_len_str: Implicit conversion from float-string to int loses precision
string(9) "untouched"
array(2) {
  [0]=>
  string(1) "w"
  [1]=>
  string(1) "w"
}
--CLEAN--
<?php
unset($e);
