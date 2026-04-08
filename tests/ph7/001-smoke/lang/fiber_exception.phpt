--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: exception thrown inside fiber propagates to caller
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
$fiber = new Fiber(function() {
    Fiber::suspend('before');
    throw new Exception('fiber error');
});

echo $fiber->start() . "\n";
try {
    $fiber->resume();
} catch (Exception $e) {
    echo "caught: " . $e->getMessage() . "\n";
}
echo "done\n";
?>
--EXPECT--
before
caught: fiber error
done
--CLEAN--
<?php
