--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: basic lifecycle (create, start, suspend, resume, return)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function fiberBody() {
    echo "fiber started\n";
    Fiber::suspend('first');
    echo "fiber resumed\n";
    Fiber::suspend('second');
    echo "fiber finishing\n";
    return 'done';
}

$fiber = new Fiber('fiberBody');

echo "before start\n";
$v1 = $fiber->start();
echo "after start: $v1\n";

$v2 = $fiber->resume();
echo "after resume: $v2\n";

$v3 = $fiber->resume();
echo "after final resume\n";
echo "return: " . $fiber->getReturn() . "\n";
?>
--EXPECT--
before start
fiber started
after start: first
fiber resumed
after resume: second
fiber finishing
after final resume
return: done
--CLEAN--
<?php
