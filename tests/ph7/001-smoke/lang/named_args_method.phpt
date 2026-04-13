--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: instance and static method calls
--FILE--
<?php
class NamCls {
    public function greet($name, $greeting) {
        echo "$greeting, $name!\n";
    }
    public static function sgreet($name, $greeting) {
        echo "static: $greeting, $name!\n";
    }
}
$obj = new NamCls();
$obj->greet(greeting: "Hello", name: "World");
NamCls::sgreet(greeting: "Hi", name: "PHP");
?>
--EXPECT--
Hello, World!
static: Hi, PHP!
--CLEAN--
<?php
