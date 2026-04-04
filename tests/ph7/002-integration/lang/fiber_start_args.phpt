--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: start() passes arguments to the callable
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function add($a, $b) {
    echo "a=$a b=$b\n";
    Fiber::suspend($a + $b);
    echo "done\n";
}

$fiber = new Fiber('add');
$sum = $fiber->start(10, 32);
echo "sum: $sum\n";
$fiber->resume();
?>
--EXPECT--
a=10 b=32
sum: 42
done
--CLEAN--
<?php
