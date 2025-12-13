--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Declaring a reserved constant name should be forbidden
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
const true = 1;
?>
--EXPECTF--
%s 2 Error: const: Cannot redeclare a reserved constant 'true'
Compile error
