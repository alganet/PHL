--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: non-iterable source throws a catchable Error
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function g() { yield from 5; }
try {
    foreach (g() as $v) {}
} catch (\Error $e) {
    echo get_class($e), "\n";
}
?>
--EXPECT--
Error
--CLEAN--
<?php
