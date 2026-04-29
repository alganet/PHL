--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke inherited from a parent class
--FILE--
<?php
class Base {
    public function __invoke($n) {
        return $n * 3;
    }
}
class Child extends Base {}
$c = new Child();
echo $c(7), "\n";
?>
--EXPECT--
21
--CLEAN--
<?php
unset($c);
