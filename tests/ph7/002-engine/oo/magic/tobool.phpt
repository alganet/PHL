--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Magic method __toBool
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class FooToBool {
    public $value = false;
    public function __toBool(){ return (bool)$this->value; }
}
$o = new FooToBool();
echo ((bool)$o ? "true" : "false") . "\n";
echo ((bool)$o ? "true" : "false") . "\n";
?>
--EXPECT--
false
false

--CLEAN--
<?php unset($o); ?>
