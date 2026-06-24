--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: constructor args, property, method
--FILE--
<?php
$o = new class(5) {
    public $x;
    function __construct($v) { $this->x = $v; }
    function dbl() { return $this->x * 2; }
};
echo $o->dbl(), "\n";
?>
--EXPECT--
10
--CLEAN--
<?php
unset($o);
