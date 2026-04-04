--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: error on double start
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function fb() {
    Fiber::suspend();
}

$fiber = new Fiber('fb');
$fiber->start();
try {
    $fiber->start();
} catch (FiberError $e) {
    echo $e->getMessage() . "\n";
}
echo "done\n";
?>
--EXPECT--
Cannot start a fiber that has already been started
done
--CLEAN--
<?php
