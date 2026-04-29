--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array spread: non-array source throws catchable \Error / \TypeError
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.0', '<')) echo 'skip Requires PHP 8.5+'; ?>
--FILE--
<?php
function spread($x) {
    return [...$x];
}

class SpreadFoo {}

$cases = [42, "hi", null, 3.14, true, false, new SpreadFoo()];
foreach ($cases as $v) {
    try {
        spread($v);
    } catch (\Throwable $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
echo "after\n";
?>
--EXPECT--
Error: Only arrays and Traversables can be unpacked, int given
Error: Only arrays and Traversables can be unpacked, string given
Error: Only arrays and Traversables can be unpacked, null given
Error: Only arrays and Traversables can be unpacked, float given
Error: Only arrays and Traversables can be unpacked, true given
Error: Only arrays and Traversables can be unpacked, false given
TypeError: Only arrays and Traversables can be unpacked, SpreadFoo given
after
--CLEAN--
<?php
