--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke forwards positional arguments and return value
--FILE--
<?php
class Adder {
    public function __invoke($x, $y) {
        return $x + $y;
    }
}
$a = new Adder();
echo $a(2, 3), "\n";
echo $a(40, 2), "\n";
?>
--EXPECT--
5
42
--CLEAN--
<?php
unset($a);
