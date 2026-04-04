--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: error on resume when not suspended
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function fb() {
    return 42;
}

$fiber = new Fiber('fb');
$fiber->start();
try {
    $fiber->resume();
} catch (FiberError $e) {
    echo $e->getMessage() . "\n";
}
echo "done\n";
?>
--EXPECT--
Cannot resume a fiber that is not suspended
done
--CLEAN--
<?php
