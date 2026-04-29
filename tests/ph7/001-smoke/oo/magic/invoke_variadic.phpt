--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke supports variadic parameters
--FILE--
<?php
class Summer {
    public function __invoke(...$xs) {
        return array_sum($xs);
    }
}
$s = new Summer();
echo $s(1, 2, 3, 4), "\n";
echo $s(), "\n";
echo $s(99), "\n";
?>
--EXPECT--
10
0
99
--CLEAN--
<?php
unset($s);
