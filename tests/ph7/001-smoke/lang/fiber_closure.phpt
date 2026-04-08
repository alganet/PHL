--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: closure callable
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
$fiber = new Fiber(function() {
    echo "closure fiber\n";
    Fiber::suspend('from closure');
    echo "closure resumed\n";
});

$v = $fiber->start();
echo "got: $v\n";
$fiber->resume();
echo "done\n";
?>
--EXPECT--
closure fiber
got: from closure
closure resumed
done
--CLEAN--
<?php
