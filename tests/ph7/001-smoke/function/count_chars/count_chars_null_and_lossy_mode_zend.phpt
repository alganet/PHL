--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php deprecates a null argument and a lossy $mode to count_chars() and answers anyway (zend half of the twin pair — PHL raises a TypeError, see count_chars_null_and_lossy_mode.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
foreach ([
    'null_string' => static fn() => count_chars(null, 3),
    'null_mode'   => static fn() => count_chars("aab", null),
    'float_mode'  => static fn() => count_chars("aab", 1.5),
    'fstr_mode'   => static fn() => count_chars("aab", "1.5"),
] as $ccnName => $ccnFn) {
    try {
        $ccnR = $ccnFn();
        echo $ccnName, ": ", is_array($ccnR) ? "array(" . count($ccnR) . ")"
                                             : var_export($ccnR, true), "\n";
    } catch (TypeError $e) {
        echo $ccnName, ": THREW\n";
    }
}
?>
--EXPECT--
null_string: ''
null_mode: array(256)
float_mode: array(2)
fstr_mode: array(2)
--CLEAN--
<?php
