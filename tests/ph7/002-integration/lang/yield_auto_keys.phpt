--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: auto-increment keys
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '5.5.0', '<')) echo 'skip Requires PHP 5.5+'; ?>
--FILE--
<?php
function gen() {
    yield 'a';
    yield 'b';
    yield 'c';
}

foreach (gen() as $k => $v) {
    echo "$k=$v\n";
}
?>
--EXPECT--
0=a
1=b
2=c
--CLEAN--
<?php
