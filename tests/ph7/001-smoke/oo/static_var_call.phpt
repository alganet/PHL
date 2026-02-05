--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Static method invocation using a class name variable
--FILE--
<?php
class StaticVarCallTest {
    public static function hello($x) { echo "hello:" . $x . "\n"; }
}

$c = 'StaticVarCallTest';
$c::hello(123);
?>
--EXPECT--
hello:123
--CLEAN--
<?php
unset($c);
