--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid class syntax
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test class with invalid method signature - abstract method in non-abstract class
class TestClass {
    abstract function abstractMethod();
}

echo "Should not reach here\n";
?>
--EXPECTF--
Should not reach here
