--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An object loosely compared with a non-object is CAST to the other operand's type: a Stringable compares as its __toString(), an int/float comparison notices and uses 1, and a refused cast (null/array/resource/no __toString) makes the object greater
--DESCRIPTION--
php's zend_compare has one rule for object-vs-non-object and it is not type
precedence: hand the OBJECT to its cast_object handler with the OTHER operand's
type and compare the result; only a REFUSED cast answers "the object is
greater". PHL fell through to its own branches instead and every one of them was
wrong somewhere:

  - a Stringable object never compared as its string — `$s == "abc"` was FALSE,
    and sort()/in_array()/array_search()/switch all inherited that;
  - an object against an int compared as two BOOLS, so `$n < 20` was FALSE where
    php compares 1 with 20;
  - an ARRAY was called greater than an object, reversing `$o <=> [1]`;
  - an object equalled every open RESOURCE, since both are truthy.

The int and float targets are the two that diagnose, and php raises E_NOTICE
there where the `(int)`/`(float)` CASTS raise E_WARNING — same sentence, two
severities, from two different places in php.

`===` is unaffected: the operand types differ, so it is false before any of this
runs.
--FILE--
<?php
function show(string $label, callable $f): void {
    try {
        $r = $f();
        echo $label, " => ";
        var_dump($r);
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
function names(array $a): string {
    return implode(",", array_map(fn ($v) => is_object($v) ? 'OBJ' : var_export($v, true), $a));
}

class Str { public function __toString(): string { return "abc"; } }
class Num { public function __toString(): string { return "10"; } }
class Bare {}

// --- string target: compares the __toString() result, with php's own
// numeric-string rules applying between the two strings afterwards.
show('eq',        fn () => (new Str()) == "abc");
show('neq',       fn () => (new Str()) != "abc");
show('identical', fn () => (new Str()) === "abc");
show('lt',        fn () => (new Str()) < "abd");
show('gt',        fn () => (new Str()) > "abb");
show('ship',      fn () => (new Str()) <=> "abc");
show('str-lhs',   fn () => "abc" == (new Str()));
show('str-lhs-gt',fn () => "abd" > (new Str()));
show('empty-str', fn () => (new Str()) == "");
show('numeric-str',   fn () => (new Num()) == "10");
show('numeric-str-2', fn () => (new Num()) == "10.0");
show('bare-vs-str',   fn () => (new Bare()) <=> "x");
show('str-vs-bare',   fn () => "x" <=> (new Bare()));

// --- int / float target: the cast cannot fail, so the object becomes 1 / 1.0
// after php's E_NOTICE (E_WARNING is what the (int)/(float) CASTS raise). The
// message names the OTHER operand's type, so an integral float still says
// "float".
show('lt-int',    fn () => (new Num()) < 20);
show('lt-int-0',  fn () => (new Num()) < 0);
show('eq-int-1',  fn () => (new Num()) == 1);
show('eq-int-10', fn () => (new Num()) == 10);
show('int-lhs',   fn () => 20 > (new Num()));
show('lt-float',  fn () => (new Num()) < 20.5);
show('ship-float',fn () => (new Num()) <=> 0.5);
show('ship-float-integral', fn () => (new Num()) <=> 20.0);
show('bare-lt-int', fn () => (new Bare()) < 20);

// --- bool target: an object is always truthy, silently.
show('eq-true',  fn () => (new Str()) == true);
show('eq-false', fn () => (new Str()) == false);
show('ship-true',fn () => (new Bare()) <=> true);

// --- refused casts: object is greater.
show('vs-null',   fn () => (new Str()) <=> null);
show('null-lhs',  fn () => null <=> (new Str()));
show('vs-array',  fn () => (new Str()) <=> [1]);
show('array-lhs', fn () => [1] <=> (new Str()));
show('lt-array',  fn () => (new Str()) < [1]);
show('vs-resource', function () {
    $h = fopen("php://memory", "r");
    $r = [(new Str()) == $h, (new Str()) <=> $h];
    fclose($h);
    return $r;
});

// --- the consumers that inherited the string bug.
show('in_array',        fn () => in_array("abc", [new Str()]));
show('in_array-flip',   fn () => in_array(new Str(), ["abc"]));
show('in_array-strict', fn () => in_array("abc", [new Str()], true));
show('array_search',    fn () => array_search("abc", [new Str()]));
show('array_keys',      fn () => array_keys(["k" => new Str()], "abc"));
show('array_unique',    fn () => count(array_unique(["abc", new Str()])));
show('switch',   function () { switch (new Str()) { case "abc": return "matched"; default: return "no"; } });
show('switch-int', function () { switch (new Num()) { case 10: return "int10"; case "10": return "str10"; default: return "none"; } });
show('sort',     function () { $a = [new Str(), "abd", "aba"]; sort($a); return names($a); });
show('rsort',    function () { $a = [new Str(), "abd", "aba"]; rsort($a); return names($a); });
show('usort',    function () { $a = ["abd", new Str(), "aba"]; usort($a, fn ($x, $y) => $x <=> $y); return names($a); });
show('max',      fn () => max(new Str(), "abd"));
show('min',      fn () => is_object(min(new Str(), "abd")) ? 'OBJ' : min(new Str(), "abd"));

// --- two objects still compare as objects, and nested-in-array comparison
// walks through to the same rule.
show('two-same',   fn () => (new Str()) == new Str());
show('two-differ', fn () => (new Str()) == new Num());
show('in-array-eq',fn () => [new Str()] == ["abc"]);
show('in-array-ship', fn () => [new Str()] <=> ["abd"]);

// --- DateTime keeps its own object-to-object comparison, and has no
// __toString, so against a string it is simply greater.
show('date-vs-date', fn () => (new DateTime("2020-01-01")) == new DateTime("2020-01-01"));
show('date-vs-str',  fn () => (new DateTime("2020-01-01")) == "2020-01-01");
show('arrayobject',  fn () => (new ArrayObject([1])) == [1]);
?>
--EXPECTF--
eq => bool(true)
neq => bool(false)
identical => bool(false)
lt => bool(true)
gt => bool(true)
ship => int(0)
str-lhs => bool(true)
str-lhs-gt => bool(true)
empty-str => bool(false)
numeric-str => bool(true)
numeric-str-2 => bool(true)
bare-vs-str => int(1)
str-vs-bare => int(-1)
PHP Notice:  Object of class Num could not be converted to int in %s on line %d
lt-int => bool(true)
PHP Notice:  Object of class Num could not be converted to int in %s on line %d
lt-int-0 => bool(false)
PHP Notice:  Object of class Num could not be converted to int in %s on line %d
eq-int-1 => bool(true)
PHP Notice:  Object of class Num could not be converted to int in %s on line %d
eq-int-10 => bool(false)
PHP Notice:  Object of class Num could not be converted to int in %s on line %d
int-lhs => bool(true)
PHP Notice:  Object of class Num could not be converted to float in %s on line %d
lt-float => bool(true)
PHP Notice:  Object of class Num could not be converted to float in %s on line %d
ship-float => int(1)
PHP Notice:  Object of class Num could not be converted to float in %s on line %d
ship-float-integral => int(-1)
PHP Notice:  Object of class Bare could not be converted to int in %s on line %d
bare-lt-int => bool(true)
eq-true => bool(true)
eq-false => bool(false)
ship-true => int(0)
vs-null => int(1)
null-lhs => int(-1)
vs-array => int(1)
array-lhs => int(-1)
lt-array => bool(false)
vs-resource => array(2) {
  [0]=>
  bool(false)
  [1]=>
  int(1)
}
in_array => bool(true)
in_array-flip => bool(true)
in_array-strict => bool(false)
array_search => int(0)
array_keys => array(1) {
  [0]=>
  string(1) "k"
}
array_unique => int(1)
switch => string(7) "matched"
PHP Notice:  Object of class Num could not be converted to int in %s on line %d
switch-int => string(5) "str10"
sort => string(15) "'aba',OBJ,'abd'"
rsort => string(15) "'abd',OBJ,'aba'"
usort => string(15) "'aba',OBJ,'abd'"
max => string(3) "abd"
min => string(3) "OBJ"
two-same => bool(true)
two-differ => bool(false)
in-array-eq => bool(true)
in-array-ship => int(-1)
date-vs-date => bool(true)
date-vs-str => bool(false)
arrayobject => bool(false)
