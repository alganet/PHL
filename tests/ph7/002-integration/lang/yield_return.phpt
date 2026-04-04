--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: return value via getReturn()
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function gen() {
    yield 1;
    yield 2;
    return 42;
}

$g = gen();
foreach ($g as $v) {
    echo "$v\n";
}
echo "return: " . $g->getReturn() . "\n";
?>
--EXPECT--
1
2
return: 42
--CLEAN--
<?php
