--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assignment to class constant attribute
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class Test {
    const MY_CONST = 42;
}
$t = new Test();
$t->MY_CONST = 123;
?>
--EXPECTF--
Error: Cannot perform assignment on a constant class attribute,PH7 is loading NULL in %s on line %d
--CLEAN--
<?php
unset($t);
