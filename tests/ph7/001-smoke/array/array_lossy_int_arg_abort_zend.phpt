--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php narrows a lossy float/float-string int argument with a deprecation and still builds the array (zend half of the twin pair — PHL raises a TypeError that aborts the call, see array_lossy_int_arg_abort.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
foreach ([
    'fill_count'     => static fn() => array_fill(1, 1.5, "v"),
    'fill_start_str' => static fn() => array_fill("1.5", 2, "v"),
    'chunk_len'      => static fn() => array_chunk([1, 2, 3], 1.5),
    'chunk_len_str'  => static fn() => array_chunk([1, 2, 3], "1.5"),
] as $aliz_name => $aliz_fn) {
    try {
        echo $aliz_name, ": ", count($aliz_fn()), "\n";
    } catch (\TypeError $e) {
        echo "THREW\n";
    }
}
?>
--EXPECT--
fill_count: 1
fill_start_str: 2
chunk_len: 3
chunk_len_str: 3
--CLEAN--
<?php
unset($e);
