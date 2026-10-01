--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Comparing two deep NON-cyclic arrays is not a recursion refusal
--FILE--
<?php
function deep($n, $leaf = 'end') {
    $root = [];
    $cur = &$root;
    for ($i = 0; $i < $n; $i++) {
        $cur[0] = [];
        $cur = &$cur[0];
    }
    $cur = $leaf;
    unset($cur);
    return $root;
}

// Only a container that is its own ANCESTOR is refused. Depth alone never is:
// php walks a 200-level pair happily, and so must every door onto the comparator.
foreach ([15, 16, 17, 32, 200] as $n) {
    try {
        echo $n, ': ', var_export(deep($n) == deep($n), true), "\n";
    } catch (Error $e) {
        echo $n, ': unexpected ', $e->getMessage(), "\n";
    }
}
try {
    echo 'differs: ', var_export(deep(40, 'a') == deep(40, 'b'), true), "\n";
} catch (Error $e) {
    echo 'differs: unexpected ', $e->getMessage(), "\n";
}
try {
    echo 'strict: ', var_export(deep(40) === deep(40), true), "\n";
} catch (Error $e) {
    echo 'strict: unexpected ', $e->getMessage(), "\n";
}
try {
    echo 'in_array: ', var_export(in_array(deep(40), [deep(40)]), true), "\n";
} catch (Error $e) {
    echo 'in_array: unexpected ', $e->getMessage(), "\n";
}
?>
--EXPECT--
15: true
16: true
17: true
32: true
200: true
differs: false
strict: true
in_array: true
