--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: expression value is the inner generator return
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function inner() {
    yield 1;
    yield 2;
    return "DONE";
}
function g() {
    $r = yield from inner();
    echo "ret=$r\n";
    yield $r;
}
echo implode(",", iterator_to_array(g(), false)), "\n";
?>
--EXPECT--
ret=DONE
1,2,DONE
--CLEAN--
<?php
