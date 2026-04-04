--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: state checking methods
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function fiberBody() {
    Fiber::suspend();
}

$fiber = new Fiber('fiberBody');

echo "before start:\n";
echo "isStarted: " . ($fiber->isStarted() ? 'yes' : 'no') . "\n";
echo "isSuspended: " . ($fiber->isSuspended() ? 'yes' : 'no') . "\n";
echo "isTerminated: " . ($fiber->isTerminated() ? 'yes' : 'no') . "\n";

$fiber->start();
echo "after start (suspended):\n";
echo "isStarted: " . ($fiber->isStarted() ? 'yes' : 'no') . "\n";
echo "isSuspended: " . ($fiber->isSuspended() ? 'yes' : 'no') . "\n";
echo "isTerminated: " . ($fiber->isTerminated() ? 'yes' : 'no') . "\n";

$fiber->resume();
echo "after resume (terminated):\n";
echo "isStarted: " . ($fiber->isStarted() ? 'yes' : 'no') . "\n";
echo "isSuspended: " . ($fiber->isSuspended() ? 'yes' : 'no') . "\n";
echo "isTerminated: " . ($fiber->isTerminated() ? 'yes' : 'no') . "\n";
?>
--EXPECT--
before start:
isStarted: no
isSuspended: no
isTerminated: no
after start (suspended):
isStarted: yes
isSuspended: yes
isTerminated: no
after resume (terminated):
isStarted: yes
isSuspended: no
isTerminated: yes
--CLEAN--
<?php
