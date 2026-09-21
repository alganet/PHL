--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: php's EXTR_REFS binds imported names by reference (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
var_dump(defined('EXTR_REFS'), EXTR_REFS);
$exr = ['a' => 1];
// EXTR_REFS aliases $a to $exr['a']: writing through $a updates the array.
var_dump(extract($exr, EXTR_REFS | EXTR_OVERWRITE));
$a = 99;
var_dump($exr['a']);
// The low byte is still validated first.
try {
    extract($exr, EXTR_REFS | EXTR_PREFIX_SAME);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
int(256)
int(1)
int(99)
extract(): Argument #3 ($prefix) is required when using this extract type
--CLEAN--
<?php
