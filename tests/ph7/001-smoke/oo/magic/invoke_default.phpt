--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke honors default parameter values
--FILE--
<?php
class InvokeGreeter {
    public function __invoke($name, $greeting = "hello") {
        return "$greeting, $name";
    }
}
$g = new InvokeGreeter();
echo $g("world"), "\n";
echo $g("world", "hi"), "\n";
?>
--EXPECT--
hello, world
hi, world
--CLEAN--
<?php
unset($g);
