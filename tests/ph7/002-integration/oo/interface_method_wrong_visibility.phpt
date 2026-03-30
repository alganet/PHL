--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface method implemented with wrong visibility produces error
--FILE--
<?php
interface Greetable {
    public function greet();
}
class Greeter implements Greetable {
    protected function greet() {
        return "Hello";
    }
}
?>
--EXPECTF--
%s %s %s  Access level to Greeter::greet() must be public (as in class Greetable) %s
--CLEAN--
<?php
