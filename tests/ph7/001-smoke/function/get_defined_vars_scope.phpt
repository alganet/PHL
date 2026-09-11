--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_vars() returns only locals in a function/method (no superglobals, no $this)
--FILE--
<?php
function gdvs_fn(int $a, int $b) {
    $c = $a + $b;
    $k = array_keys(get_defined_vars());
    sort($k);
    return implode(',', $k);
}
class GdvsK {
    public function m(int $x) {
        $y = 1;
        $k = array_keys(get_defined_vars());
        sort($k);
        return implode(',', $k);
    }
}
echo gdvs_fn(1, 2), "\n";
echo (new GdvsK())->m(5), "\n";
?>
--EXPECT--
a,b,c
x,y
