--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multiple use statements each with their own adaptation block
--FILE--
<?php
trait Greet {
    public function hello() { return "Hello"; }
    public function bye() { return "Bye"; }
}
trait Count {
    public function next() { return 1; }
    public function prev() { return -1; }
}
class MyClass {
    use Greet {
        hello as protected secretHello;
    }
    use Count {
        next as public getNext;
    }
    public function test() {
        return $this->secretHello() . " " . $this->getNext();
    }
}
$obj = new MyClass();
echo $obj->hello(), "\n";
echo $obj->bye(), "\n";
echo $obj->next(), "\n";
echo $obj->prev(), "\n";
echo $obj->test(), "\n";
echo $obj->getNext(), "\n";
?>
--EXPECT--
Hello
Bye
1
-1
Hello 1
1
--CLEAN--
<?php
