--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid attribute syntax
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class TestClass {
    public $var 123;
}
?>
--EXPECTF--
%s 3 Error: Expected '=' or ';' after attribute name 'var'
Compile error