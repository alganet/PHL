--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Method names are case-insensitive in php: $o->FOO() finds foo(), and method_exists agrees
--FILE--
<?php
class MethodCase {
    function foo() { return 'inst'; }
    static function bar() { return 'stat'; }
}
$o = new MethodCase();
echo $o->FOO(), "\n";
echo $o->Foo(), "\n";
echo MethodCase::BAR(), "\n";
var_dump(method_exists('MethodCase', 'FOO'));
var_dump(method_exists('MethodCase', 'foo'));
var_dump(is_callable([$o, 'FoO']));

// Overriding across classes still matches on the same rule.
class MethodCaseChild extends MethodCase {
    function FOO() { return 'child'; }
}
$c = new MethodCaseChild();
echo $c->foo(), "\n";
?>
--EXPECT--
inst
inst
stat
bool(true)
bool(true)
bool(true)
child
--CLEAN--
<?php
unset($o, $c);
