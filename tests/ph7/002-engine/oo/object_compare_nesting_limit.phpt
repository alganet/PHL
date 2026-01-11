--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison nesting limit
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
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
--EXPECTF--
%s Error: Nesting limit reached: Infinite recursion?
not equal