--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Foreach iteration on object properties
--FILE--
<?php
class A {
    public $a = 7;
    public $b = 9;
}
$o = new A();
foreach($o as $k => $v){
    echo $k . ':' . $v . "\n";
}
?>
--EXPECT--
a:7
b:9
--CLEAN--
<?php
unset($o);
