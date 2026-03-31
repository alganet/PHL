--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface method with extra parameters with defaults is allowed
--FILE--
<?php
interface Greetable {
    public function greet($name);
}
class Greeter implements Greetable {
    public function greet($name, $greeting = "Hello") {
        return "$greeting $name";
    }
}
$g = new Greeter();
echo $g->greet("World") . "\n";
echo $g->greet("World", "Hi") . "\n";
?>
--EXPECT--
Hello World
Hi World
--CLEAN--
<?php
