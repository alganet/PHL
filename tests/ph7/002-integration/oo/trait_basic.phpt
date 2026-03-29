--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Basic trait usage: single trait with method and property
--FILE--
<?php
trait Greet {
    public function hello() {
        return "Hello";
    }
}

trait Counter {
    private $count = 0;
    public function increment() {
        $this->count++;
        return $this->count;
    }
}

class MyClass {
    use Greet;
    use Counter;
}

$obj = new MyClass();
echo $obj->hello(), "\n";
echo $obj->increment(), "\n";
echo $obj->increment(), "\n";
?>
--EXPECT--
Hello
1
2
--CLEAN--
<?php
