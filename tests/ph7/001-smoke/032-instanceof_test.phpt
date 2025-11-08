--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Instanceof operator
--FILE--
<?php
class InstanceofTestClass {
    public $value = 42;
}
$obj = new InstanceofTestClass();
echo $obj instanceof InstanceofTestClass ? "yes" : "no"; // yes
echo "\n";
$not_obj = "string";
echo $not_obj instanceof InstanceofTestClass ? "yes" : "no"; // no
?>
--EXPECT--
yes
no
--CLEAN--
<?php
unset($obj, $not_obj);
?>