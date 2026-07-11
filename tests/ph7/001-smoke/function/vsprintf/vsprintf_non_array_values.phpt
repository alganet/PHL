--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
vsprintf()/vprintf() throw TypeError when $values is not an array (PHP 8)
--FILE--
<?php
foreach ([5, "x", 3.2, null, true] as $v) {
    try {
        vsprintf("%d", $v);
        echo "NO_ERROR\n";
    } catch (\TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
try {
    vprintf("%d", 7);
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
// A real array still works.
echo vsprintf("%d-%s", [1, "ok"]), "\n";
?>
--EXPECT--
vsprintf(): Argument #2 ($values) must be of type array, int given
vsprintf(): Argument #2 ($values) must be of type array, string given
vsprintf(): Argument #2 ($values) must be of type array, float given
vsprintf(): Argument #2 ($values) must be of type array, null given
vsprintf(): Argument #2 ($values) must be of type array, true given
vprintf(): Argument #2 ($values) must be of type array, int given
1-ok
--CLEAN--
<?php
