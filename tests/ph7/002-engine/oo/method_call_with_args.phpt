--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object method call with arguments
--FILE--
<?php
class Greeter {
    public function greet($name) {
        return "Hello, " . $name . "!";
    }
}

$greeter = new Greeter();
echo $greeter->greet("World");
?>
--EXPECT--
Hello, World!

--CLEAN--
<?php
// Nothing to clean
?>