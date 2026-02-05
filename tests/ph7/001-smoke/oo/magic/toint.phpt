--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Magic method __toInt
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class FooToInt {
    public $value = 42;
    public function __toInt(){ return (int)$this->value; }
}
$o = new FooToInt();
echo (int)$o . "\n";
echo intval($o) . "\n";
?>
--EXPECT--
42
42
--CLEAN--
<?php
unset($o);
