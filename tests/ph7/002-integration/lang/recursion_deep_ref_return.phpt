--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap): by-reference return threaded through the chain
--FILE--
<?php
$store = ['v' => 0];
function &walk(array &$a, int $n) {
    if ($n === 0) {
        return $a['v'];
    }
    return walk($a, $n - 1);
}
$r = &walk($store, 20);
$r = 42;
echo $store['v'], "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
