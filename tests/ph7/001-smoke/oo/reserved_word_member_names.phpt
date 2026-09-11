--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
reserved words (null/true/false/array/...) are valid member names after :: and ->
--FILE--
<?php
/* php allows any reserved word as a class constant, enum case, or method name,
 * accessed after :: / ->. null/true/false are lexed as bare identifiers, so
 * `Enum::Null` used to fold to the literal VALUE (null) and left an empty member
 * name ("Access to undeclared static property Enum::$"). Regression for PHPUnit's
 * NativeType enum (cases Null, Array, Object, String, Iterable, ...). */
enum NrwType: string {
    case Null     = 'null';
    case Array    = 'array';
    case Object   = 'object';
    case String   = 'string';
    case Iterable = 'iterable';
    case True     = 'true';
}
echo NrwType::Null->value, ",", NrwType::Array->value, ",", NrwType::Object->value, "\n";
echo NrwType::from('string')->name, "\n";
var_dump(NrwType::True instanceof NrwType);

class NrwMethods {
    public function list(): string  { return "L"; }
    public function print(): string { return "P"; }
    public function throw(): string { return "T"; }
    public function and(): string   { return "A"; }
}
$m = new NrwMethods;
echo $m->list(), $m->print(), $m->throw(), $m->and(), "\n";

/* Plain null/true/false/array must still evaluate as VALUES, not names. */
$n = null; $t = true; $f = false;
var_dump($n === null, $t, $f);
$arr = array(1, 2, 3);
echo count($arr), "\n";
?>
--EXPECT--
null,array,object
String
bool(true)
LPTA
bool(true)
bool(true)
bool(false)
3
--CLEAN--
<?php
