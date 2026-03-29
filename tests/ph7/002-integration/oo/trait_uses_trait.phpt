--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait uses another trait
--FILE--
<?php
trait Hello {
    public function hello() { return "Hello"; }
}
trait Greet {
    use Hello;
    public function greet() { return $this->hello() . " World"; }
}
class MyClass {
    use Greet;
}
$obj = new MyClass();
echo $obj->hello(), "\n";
echo $obj->greet(), "\n";
?>
--EXPECT--
Hello
Hello World
--CLEAN--
<?php
