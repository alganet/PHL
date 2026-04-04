--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return type declarations on interface methods
--FILE--
<?php
interface Greeter {
    public function greet(): string;
}

class Hello implements Greeter {
    public function greet(): string { return "Hello!"; }
}

echo (new Hello())->greet() . "\n";
?>
--EXPECT--
Hello!
--CLEAN--
<?php

