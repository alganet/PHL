--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap) composed with generators: deep call in body, recursive yield from, resume from depth
--FILE--
<?php
function helper(int $n): int {
    return $n === 0 ? 0 : 1 + helper($n - 1);
}
function inner_gen(int $n): Generator {
    if ($n > 0) {
        yield $n;
        yield from inner_gen($n - 1);
    }
}
function gen(): Generator {
    yield helper(20);
    yield from inner_gen(5);
    yield "last";
}
foreach (gen() as $v) {
    echo $v, ",";
}
echo "\n";
function pump(Generator $g, int $n) {
    if ($n === 0) {
        $g->next();
        return $g->current();
    }
    return pump($g, $n - 1);
}
function gen2(): Generator {
    yield "a";
    yield "resumed-deep";
}
$g = gen2();
$g->current();
echo pump($g, 15), "\n";
?>
--EXPECT--
20,5,4,3,2,1,last,
resumed-deep
--CLEAN--
<?php
