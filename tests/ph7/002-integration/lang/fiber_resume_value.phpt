--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: resume value passed to Fiber::suspend() return
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function fiberBody() {
    $received = Fiber::suspend('hello');
    echo "received: $received\n";
    $received2 = Fiber::suspend('world');
    echo "received2: $received2\n";
    return 'finished';
}

$fiber = new Fiber('fiberBody');

$v1 = $fiber->start();
echo "suspend1: $v1\n";

$v2 = $fiber->resume('value1');
echo "suspend2: $v2\n";

$fiber->resume('value2');
echo "return: " . $fiber->getReturn() . "\n";
?>
--EXPECT--
suspend1: hello
received: value1
suspend2: world
received2: value2
return: finished
--CLEAN--
<?php
