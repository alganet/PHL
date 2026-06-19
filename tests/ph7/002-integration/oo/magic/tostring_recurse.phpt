--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object __toString recursion limit
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class A {
    function __toString() {
        return (string)$this;
    }
}

$a = new A();
echo $a;
?>
--EXPECTF--
%s Error:  Maximum recursion depth of %d reached
Object
--CLEAN--
<?php
unset($a);
