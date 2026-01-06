--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
OO constructor visibility (private constructors are made public)
--FILE--
<?php
class ConstructorVisibilityTest {
    private function __construct() {
        echo "Private constructor called\n";
    }
    
    private function __destruct() {
        echo "Private destructor called\n";
    }
}

echo "Testing constructor visibility...\n";

$test = new ConstructorVisibilityTest();
unset($test);

echo "Constructor visibility test completed\n";
?>
--CLEAN--
<?php
unset($test);
?>
--EXPECTF--
Testing constructor visibility...
Private constructor called
Private destructor called
Constructor visibility test completed