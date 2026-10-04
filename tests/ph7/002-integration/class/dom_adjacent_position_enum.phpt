--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced DOM API's insertion-point enum, and its exception's second name
--FILE--
<?php
/* Dom\AdjacentPosition is STRING-backed, and its four backing values are the
 * HTML standard's lowercase spellings rather than the case names -- so the
 * pairing, not just the four names, is the contract: a program that spells a
 * position by hand is reading the standard. `from('BeforeBegin')` is a
 * ValueError and `from('beforebegin')` is the case.
 *
 * An enum declared from C is not stamped final the way a compiled one is, so
 * isFinal()/getModifiers() answer false/0 here.
 *
 * Dom\DOMException is the 2004 DOMException under a second name and not a
 * class of its own: get_class() answers the plain name, and either spelling
 * catches what the other throws. */
$r = new ReflectionEnum('Dom\AdjacentPosition');
echo "backing=", (string)$r->getBackingType(), " final=", var_export($r->isFinal(), true),
     " mods=", $r->getModifiers(), " enum_exists=",
     var_export(enum_exists('Dom\AdjacentPosition'), true), "\n";
foreach ($r->getCases() as $c) {
    printf("%s => %s\n", $c->getName(), var_export($c->getBackingValue(), true));
}
var_dump($r->getInterfaceNames());
var_dump(Dom\AdjacentPosition::cases());
var_dump(Dom\AdjacentPosition::from('beforeend'));
var_dump(Dom\AdjacentPosition::tryFrom('nope'));
try {
    Dom\AdjacentPosition::from('BeforeBegin');
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
var_dump(Dom\AdjacentPosition::AfterEnd->value, Dom\AdjacentPosition::AfterEnd->name);
echo json_encode([Dom\AdjacentPosition::AfterEnd, Dom\AdjacentPosition::BeforeBegin]), "\n";
var_dump(Dom\AdjacentPosition::BeforeBegin instanceof BackedEnum,
         Dom\AdjacentPosition::BeforeBegin === Dom\AdjacentPosition::from('beforebegin'));
var_dump(\Dom\AdjacentPosition::AfterBegin);

$e = new Dom\DOMException("boom", 3);
var_dump(get_class($e), $e->getMessage(), $e->getCode(), $e instanceof DOMException);
var_dump(class_exists('Dom\DOMException'), class_exists('DOM\domexception'));
try {
    throw new DOMException("x", 1);
} catch (Dom\DOMException $e) {
    echo "caught as namespaced: ", get_class($e), "\n";
}
var_dump((new ReflectionClass('Dom\DOMException'))->getName());
?>
--EXPECT--
backing=string final=false mods=0 enum_exists=true
BeforeBegin => 'beforebegin'
AfterBegin => 'afterbegin'
BeforeEnd => 'beforeend'
AfterEnd => 'afterend'
array(2) {
  [0]=>
  string(10) "BackedEnum"
  [1]=>
  string(8) "UnitEnum"
}
array(4) {
  [0]=>
  enum(Dom\AdjacentPosition::BeforeBegin)
  [1]=>
  enum(Dom\AdjacentPosition::AfterBegin)
  [2]=>
  enum(Dom\AdjacentPosition::BeforeEnd)
  [3]=>
  enum(Dom\AdjacentPosition::AfterEnd)
}
enum(Dom\AdjacentPosition::BeforeEnd)
NULL
ValueError: "BeforeBegin" is not a valid backing value for enum Dom\AdjacentPosition
string(8) "afterend"
string(8) "AfterEnd"
["afterend","beforebegin"]
bool(true)
bool(true)
enum(Dom\AdjacentPosition::AfterBegin)
string(12) "DOMException"
string(4) "boom"
int(3)
bool(true)
bool(true)
bool(true)
caught as namespaced: DOMException
string(12) "DOMException"
