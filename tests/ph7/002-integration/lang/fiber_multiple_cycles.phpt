--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: multiple suspend/resume cycles with counter
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function counter() {
    $i = 0;
    while (true) {
        Fiber::suspend($i);
        $i++;
    }
}

$fiber = new Fiber('counter');

echo $fiber->start() . "\n";
echo $fiber->resume() . "\n";
echo $fiber->resume() . "\n";
echo $fiber->resume() . "\n";
echo $fiber->resume() . "\n";
?>
--EXPECT--
0
1
2
3
4
--CLEAN--
<?php
