--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: independent generator instances do not clash
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function g() { yield from [1, 2, 3]; }
$a = g(); $b = g();
$a->current(); $b->current();
$a->next();
echo $a->current(), " ", $b->current(), "\n";
?>
--EXPECT--
2 1
--CLEAN--
<?php
