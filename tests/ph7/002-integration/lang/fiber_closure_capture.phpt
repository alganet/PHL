--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: closure capturing variables from outer scope
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
$greeting = "hello";
$count = 3;

$fiber = new Fiber(function() use ($greeting, $count) {
    for ($i = 0; $i < $count; $i++) {
        Fiber::suspend("$greeting $i");
    }
    return "done after $count";
});

echo $fiber->start() . "\n";
echo $fiber->resume() . "\n";
echo $fiber->resume() . "\n";
$fiber->resume();
echo $fiber->getReturn() . "\n";
?>
--EXPECT--
hello 0
hello 1
hello 2
done after 3
--CLEAN--
<?php
