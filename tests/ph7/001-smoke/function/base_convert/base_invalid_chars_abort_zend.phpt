--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php skips invalid base-conversion characters and returns a value (zend half of the twin pair — PHL raises a ValueError that aborts the call, see base_invalid_chars_abort.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$bicz_r = "untouched";
foreach ([
    'bindec'  => static fn() => bindec("10z1"),
    'octdec'  => static fn() => octdec("178"),
    'hexdec'  => static fn() => hexdec("1g2"),
    'baseconv' => static fn() => base_convert("12z", 16, 10),
] as $bicz_name => $bicz_fn) {
    try {
        $bicz_r = $bicz_fn();
        echo $bicz_name, ": ", var_export($bicz_r, true), "\n";
    } catch (\ValueError $e) {
        echo "THREW\n";
    }
}
?>
--EXPECT--
bindec: 5
octdec: 15
hexdec: 18
baseconv: '18'
--CLEAN--
<?php
unset($e);
