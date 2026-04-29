--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke handles by-reference parameters
--FILE--
<?php
class Bumper {
    public function __invoke(&$x) {
        $x++;
    }
}
$b = new Bumper();
$v = 10;
$b($v);
$b($v);
echo $v, "\n";
?>
--EXPECT--
12
--CLEAN--
<?php
unset($b, $v);
