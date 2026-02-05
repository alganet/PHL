--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dynamic method name invocation
--FILE--
<?php
class D {
    public $value = 5;
    public function inc($n) { $this->value += $n; }
}
$obj = new D();
$name = 'inc';
$obj->$name(3);
echo $obj->value . "\n";
?>
--EXPECT--
8
--CLEAN--
<?php
unset($obj, $name);
