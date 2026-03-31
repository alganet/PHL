--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface method with extra parameters without defaults produces error
--FILE--
<?php
interface Greetable {
    public function greet($name);
}
class Greeter implements Greetable {
    public function greet($name, $greeting) {
        return "$greeting $name";
    }
}
?>
--EXPECTF--
%s %s %s  Declaration of Greeter::greet($name, $greeting) must be compatible with Greetable::greet($name) %s
--CLEAN--
<?php
