--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An anonymous class with a parent, an interface or a trait is declared where its new runs
--DESCRIPTION--
php links an anonymous class at compile time only when it has no parent, no
interface and no trait; __toString counts, since it brings Stringable. Any
other one is linked by the expression itself, and get_declared_classes() lists
it only once that has run -- in compile order, not run order.
--FILE--
<?php
function acdw_anons() {
    $out = [];
    foreach (get_declared_classes() as $c) {
        if (str_contains($c, "@anonymous\0")) {
            $out[] = strstr($c, "\0", true) . ':' . explode('$', substr($c, strrpos($c, ':') + 1))[0];
        }
    }
    return implode(',', $out);
}
interface AcdwI {}
trait AcdwT { function t() { return 't'; } }
abstract class AcdwP { abstract function f(); }
function acdw_second() { return new class implements AcdwI {}; }
function acdw_first() { return new class extends ArrayObject {}; }
function acdw_plain() { return new class {}; }
echo acdw_anons(), "\n";
if (0) { new class extends AcdwP { function f() {} }; }
$s = new class { function __toString(): string { return "s"; } };
echo acdw_anons(), "\n";
$x = acdw_first();
$y = acdw_second();
echo acdw_anons(), "\n";
for ($i = 0; $i < 3; $i++) {
    $o[] = new class { use AcdwT; };
}
echo acdw_anons(), "\n";
var_dump(get_class($o[0]) === get_class($o[2]), $o[1]->t());
eval('$e = new class implements AcdwI {};');
echo acdw_anons(), "\n";
var_dump($y instanceof AcdwI, $x instanceof ArrayObject, (string)$s);
var_dump(class_exists(get_class($y), false), (new ReflectionClass($x))->isAnonymous());
--EXPECT--
class@anonymous:16
class@anonymous:16,class@anonymous:19
AcdwI@anonymous:14,ArrayObject@anonymous:15,class@anonymous:16,class@anonymous:19
AcdwI@anonymous:14,ArrayObject@anonymous:15,class@anonymous:16,class@anonymous:19,class@anonymous:25
bool(true)
string(1) "t"
AcdwI@anonymous:14,ArrayObject@anonymous:15,class@anonymous:16,class@anonymous:19,class@anonymous:25,AcdwI@anonymous:1
bool(true)
bool(true)
string(1) "s"
bool(true)
bool(true)
