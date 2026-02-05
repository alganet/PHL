--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Throw from a class constructor and catch it
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class TestCtorThrow {
    public function __construct(){
        throw new Exception("ctor fail");
    }
}

try {
    $o = new TestCtorThrow();
    echo "no\n"; // This will execute, incorrectly in PH7
} catch (Exception $e) {
    echo "caught: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
caught: ctor fail
no
--CLEAN--
<?php
// PH7 does not properly catch exceptions in constructors, it keeps going on
unset($o);
