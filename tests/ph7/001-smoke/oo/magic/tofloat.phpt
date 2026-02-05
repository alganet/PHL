--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Magic method __toFloat
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class FooToFloat {
    public $value = 3.14;
    public function __toFloat(){ return (float)$this->value; }
}
$o = new FooToFloat();
echo (float)$o . "\n";
echo floatval($o) . "\n";
?>
--EXPECT--
3.14
3.14
--CLEAN--
<?php
unset($o);
