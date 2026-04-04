--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: function with arguments
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '5.5.0', '<')) echo 'skip Requires PHP 5.5+'; ?>
--FILE--
<?php
function range_gen($start, $end) {
    for ($i = $start; $i <= $end; $i++) {
        yield $i;
    }
}

foreach (range_gen(3, 7) as $v) {
    echo "$v\n";
}
?>
--EXPECT--
3
4
5
6
7
--CLEAN--
<?php
