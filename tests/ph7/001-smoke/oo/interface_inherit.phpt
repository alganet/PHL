--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface inheritance: interface extends interface and class implements it
--FILE--
<?php
interface BaseInterface {
    public function foo();
    const BASE_CONST = 42;
}

interface DerivedInterface extends BaseInterface {
}

class Impl implements DerivedInterface {
    public function foo() { return "ok"; }
}

$instance = new Impl();
echo $instance->foo() . "\n";
echo "CONST: " . DerivedInterface::BASE_CONST . "\n";
?>
--EXPECT--
ok
CONST: 42
--CLEAN--
<?php
unset($instance);
