--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Everything that NAMES a trait member's class names the class that composed it
--DESCRIPTION--
The other half of the trait-scope rule: not what code may reach, but what the
engine calls it. A frame running a trait method reports the composing class in
debug_backtrace(); Reflection reports it as the member's declaring class;
var_dump/print_r put it in the `["p":"C":private]` annotation; and the MANGLED
key that annotation reads -- the one the (array) cast and serialize() are built
on -- carries it, so a serialize() string named the TRAIT and was not the one php
writes. The Errors name it too, readonly and uninitialized-typed alike. Each site
had its own `pDeclClass ? pDeclClass : theClass` and none of them knew about
traits; they ask one helper now.
--FILE--
<?php
trait TsnT {
    private $priv = 1;
    protected $prot = 2;
    public readonly int $ro;
    public int $typed;
    public function __construct() { $this->ro = 7; }
    public function where() { $f = debug_backtrace()[0]; return [$f['class'], $f['type']]; }
    public static function whereStatic() { $f = debug_backtrace()[0]; return [$f['class'], $f['type']]; }
}
class TsnC { use TsnT; }
class TsnKid extends TsnC {}

/* A frame running a trait method reports the class php composed it into. */
var_dump((new TsnC)->where(), (new TsnKid)->where(), TsnC::whereStatic());

/* Reflection reports the composing class as the member's declarer. */
$r = new ReflectionClass('TsnC');
var_dump($r->getProperty('priv')->class,
         $r->getProperty('priv')->getDeclaringClass()->getName(),
         $r->getMethod('where')->getDeclaringClass()->getName());

/* var_dump / print_r name it in the private annotation, and the MANGLED key every
 * wire format is built on carries it too. */
$c = new TsnC;
print_r($c); /* var_dump would print an object HANDLE, which never matches across engines */
foreach ((array)$c as $k => $v) { var_dump(str_replace("\0", '|', $k)); }
var_dump(str_replace("\0", '|', serialize($c)));
$back = unserialize(serialize($c));
var_dump($back == $c);

/* And the Errors name it: readonly, and an uninitialized typed property. */
try { $c->ro = 8; } catch (Throwable $e) { var_dump($e->getMessage()); }
try { $c->typed; } catch (Throwable $e) { var_dump($e->getMessage()); }
?>
--EXPECT--
array(2) {
  [0]=>
  string(4) "TsnC"
  [1]=>
  string(2) "->"
}
array(2) {
  [0]=>
  string(4) "TsnC"
  [1]=>
  string(2) "->"
}
array(2) {
  [0]=>
  string(4) "TsnC"
  [1]=>
  string(2) "::"
}
string(4) "TsnC"
string(4) "TsnC"
string(4) "TsnC"
TsnC Object
(
    [priv:TsnC:private] => 1
    [prot:protected] => 2
    [ro] => 7
)
string(10) "|TsnC|priv"
string(7) "|*|prot"
string(2) "ro"
string(68) "O:4:"TsnC":3:{s:10:"|TsnC|priv";i:1;s:7:"|*|prot";i:2;s:2:"ro";i:7;}"
bool(true)
string(41) "Cannot modify readonly property TsnC::$ro"
string(70) "Typed property TsnC::$typed must not be accessed before initialization"
--CLEAN--
<?php
