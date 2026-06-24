--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: array delegation expression value is null
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function g() {
    $r = yield from [1, 2];
    var_export($r);
    echo "\n";
}
foreach (g() as $v) {}
?>
--EXPECT--
NULL
--CLEAN--
<?php
