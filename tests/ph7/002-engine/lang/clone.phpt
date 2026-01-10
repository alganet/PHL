--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Clone keyword
--FILE--
<?php
class Test {
    public $value = 1;
}
$obj = new Test();
$clone = clone $obj;
echo $clone->value . "\n";
?>
--EXPECT--
1