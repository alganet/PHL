--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Comparing two deep NON-cyclic object graphs is not a recursion refusal
--FILE--
<?php
class Test {
    public $prop;
}

function chain($n) {
    $head = new Test();
    $current = $head;
    for ($i = 0; $i < $n; $i++) {
        $next = new Test();
        $current->prop = $next;
        $current = $next;
    }
    return $head;
}

// php has no depth limit on comparison: only a container that is its own
// ANCESTOR is refused, and a chain 35 (or 200) links long is merely deep.
try {
    echo (chain(35) == chain(35)) ? "equal\n" : "not equal\n";
} catch (Error $e) {
    echo "unexpected: " . $e->getMessage() . "\n";
}
try {
    echo (chain(200) == chain(200)) ? "equal\n" : "not equal\n";
} catch (Error $e) {
    echo "unexpected: " . $e->getMessage() . "\n";
}
// A difference at the deep end is still found.
$a = chain(40);
$b = chain(40);
$c = $b;
for ($i = 0; $i < 40; $i++) { $c = $c->prop; }
$c->prop = new Test();
try {
    echo ($a == $b) ? "equal\n" : "not equal\n";
} catch (Error $e) {
    echo "unexpected: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
equal
equal
not equal
