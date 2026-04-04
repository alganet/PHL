--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return type declarations with class names and self
--FILE--
<?php
class Foo {
    public function getSelf(): self { return $this; }
    public function name(): string { return "Foo"; }
}

$f = new Foo();
echo $f->getSelf()->name() . "\n";

abstract class Base {
    abstract public function compute(): int;
}

class Derived extends Base {
    public function compute(): int { return 99; }
}

echo (new Derived())->compute() . "\n";
?>
--EXPECT--
Foo
99
--CLEAN--
<?php

