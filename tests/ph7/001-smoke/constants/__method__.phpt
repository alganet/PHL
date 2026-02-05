--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: __METHOD__ magic constant
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "__METHOD__ in global scope: '" . __METHOD__ . "'\n";
function test_func() {
    echo "__METHOD__ in function: '" . __METHOD__ . "'\n";
}
test_func();
class TestClass {
    public function test_method() {
        echo "__METHOD__ in method: '" . __METHOD__ . "'\n";
    }
}
$obj = new TestClass();
$obj->test_method();
?>
--EXPECT--
__METHOD__ in global scope: ''
__METHOD__ in function: ''
__METHOD__ in method: 'test_method'
--CLEAN--
<?php
unset($obj);
