--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: PHL rejects the EXTR_REFS bit loudly (a recorded divergence)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
// php's EXTR_REFS (256) binds every imported name BY REFERENCE to its array
// slot. PHL has no by-reference extraction; importing by value instead would
// silently drop the write-back the caller asked for, so the bit is refused.
// The EXTR_REFS constant itself stays undefined (loud on its own).
var_dump(defined('EXTR_REFS'));
$exr = ['a' => 1];
foreach ([256, 256 | 1, 256 | 3] as $flags) {
    try {
        var_dump(extract($exr, $flags, 'p'));
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
// The low byte is still validated first, so the php-exact errors keep winning.
try {
    extract($exr, 256 | 2);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(false)
extract(): Argument #2 ($flags) EXTR_REFS is not supported
extract(): Argument #2 ($flags) EXTR_REFS is not supported
extract(): Argument #2 ($flags) EXTR_REFS is not supported
extract(): Argument #3 ($prefix) is required when using this extract type
--CLEAN--
<?php
