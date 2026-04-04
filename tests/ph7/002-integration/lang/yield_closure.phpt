--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: yield inside closure (anonymous function)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '5.5.0', '<')) echo 'skip Requires PHP 5.5+'; ?>
--FILE--
<?php
$gen = function($n) {
    for ($i = 1; $i <= $n; $i++) {
        yield $i * $i;
    }
};

foreach ($gen(4) as $v) {
    echo "$v\n";
}
?>
--EXPECT--
1
4
9
16
--CLEAN--
<?php
