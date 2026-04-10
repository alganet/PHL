--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 indented heredoc with variable interpolation
--FILE--
<?php
$name = "World";
$n = 42;
$x = <<<EOT
    Hello, $name!
    n = $n
    EOT;
echo "[$x]\n";

class Box {
    public $label = "hi";
}
$b = new Box();
$y = <<<EOT
    label={$b->label}
    EOT;
echo "[$y]\n";
--EXPECT--
[Hello, World!
n = 42]
[label=hi]
--CLEAN--
<?php
unset($name, $n, $x, $b, $y);
