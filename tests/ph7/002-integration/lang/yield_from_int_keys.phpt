--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: inner int keys preserved, outer auto-key continues
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function g() {
    yield "first";
    yield from [0 => "inner"];
    yield "after";
}
foreach (g() as $k => $v) { echo "$k => $v\n"; }
?>
--EXPECT--
0 => first
0 => inner
1 => after
--CLEAN--
<?php
