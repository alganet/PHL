--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class method overrides trait method
--FILE--
<?php
trait Greeter {
    public function greet() { return "trait"; }
}
class MyClass {
    use Greeter;
    public function greet() { return "class"; }
}
$obj = new MyClass();
echo $obj->greet(), "\n";
?>
--EXPECT--
class
--CLEAN--
<?php
