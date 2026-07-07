--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
20k-deep mutual recursion (function <-> method <-> closure) completes (BYTECODE.md stage 3)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only deep-recursion probe: depth exceeds the php oracle stack / xdebug nesting limit'; ?>
--FILE--
<?php
class Hop {
    public function viaMethod(int $n): int {
        return viaFunction($n - 1);
    }
}
function viaFunction(int $n): int {
    global $viaClosure;
    if ($n <= 0) {
        return 0;
    }
    return 1 + $viaClosure($n);
}
$hop = new Hop();
$viaClosure = function (int $n) use ($hop): int {
    return 1 + $hop->viaMethod($n);
};
// each round trips function -> closure -> method: 3 frames per unit
echo "units=", viaFunction(20000), "\n";
?>
--EXPECT--
units=40000
--CLEAN--
<?php
unset($hop, $viaClosure);
