--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison nesting limit
--SKIPIF--
<?php
// Comparing two deeply nested object graphs trips PHL's nesting guard
// ("Nesting limit reached: Infinite recursion?"). php has no comparison depth limit and
// recurses until it exhausts memory, so there is no shared expectation to write.
if (function_exists('zend_version')) { echo 'skip PHL bounds comparison nesting; php recurses until memory exhaustion'; }
?>
--FILE--
<?php
class Test {
    public $prop;
}

$a = new Test();
$current = $a;
for ($i = 0; $i < 35; $i++) {
    $next = new Test();
    $current->prop = $next;
    $current = $next;
}

$b = new Test();
$current = $b;
for ($i = 0; $i < 35; $i++) {
    $next = new Test();
    $current->prop = $next;
    $current = $next;
}

if ($a == $b) {
    echo "equal\n";
} else {
    echo "not equal\n";
}
?>
--EXPECT--
not equal
--EXPECT_STDERR--
PHP Error:  Nesting limit reached: Infinite recursion? in %s on line %d
--CLEAN--
<?php
unset($a, $current, $next, $b);
