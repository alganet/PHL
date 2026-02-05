--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Member assignment
--FILE--
<?php
class Test {
    public $prop;
}
$obj = new Test();
$obj->prop = 'value';
echo $obj->prop . "\n";
?>
--EXPECT--
value
--CLEAN--
<?php
unset($obj);
