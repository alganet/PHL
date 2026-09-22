--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static property belongs to the class, so an instance never exposes it
--DESCRIPTION--
php's static properties live on the class: an object never carries one, and
reaching for `$o->s` finds nothing -- it notices `Accessing static property
C::$s as non static`, then treats the name as a MISSING property (a read warns
and answers null, isset() is false, a write goes to a dynamic property). PHL
gives every instance an attribute-table entry for every declared member,
statics included (they share the class slot), and three raw surfaces plus the
instance fetch had no filter: `(array)$o` and `foreach ($o as ...)` listed the
static, `$o->s` READ it, and -- the silent wrong answer that matters -- `$o->s
= 'x'` WROTE the class's own static through an object. var_dump/print_r/
get_object_vars/json_encode/serialize already filtered it; they are asserted
here so all six surfaces stay one rule. The fetch checks VISIBILITY first, as
php does (a private static through an instance is the ordinary access Error),
and stays silent for a class that declares the magic accessor the context would
dispatch (php passes `silent = ce->__get/__set/__unset != NULL` to its own
lookup).
--FILE--
<?php
class SnoBase {
    public static $s = 'st';
    const K = 'kk';
    public $p = 1;
    protected $q = 2;
}
$o = new SnoBase;

echo "== the raw surfaces list only the object's own properties ==\n";
var_dump((array)$o);
var_dump(get_object_vars($o));
var_dump($o);
print_r($o);
echo "\n";
var_dump(json_encode($o));
echo "== foreach ==\n";
foreach ($o as $k => $v) { echo "  ", $k, " => ", $v, "\n"; }

echo "== reading it through the instance ==\n";
var_dump($o->s);

echo "== isset()/empty() are silent ==\n";
var_dump(isset($o->s), empty($o->s));

echo "== writing through the instance leaves the CLASS static alone ==\n";
try {
    $o->s = 'x';
} catch (Error $e) {
    /* PHL rejects dynamic-property creation that php only performs (policy §10);
     * either way the class's static must not move. */
}
var_dump(SnoBase::$s);

echo "== a by-reference argument cannot reach the class slot either ==\n";
class SnoRef { public static $s = 'st'; }
function sno_byref(&$x) { $x = 'PWNED'; }
$r = new SnoRef;
try {
    sno_byref($r->s);
} catch (Error $e) {
    /* §10 again: php binds a fresh dynamic property, PHL refuses to create one. */
}
var_dump(SnoRef::$s);

echo "== a NON-PUBLIC static is an access Error, as for any private member ==\n";
class SnoPriv { private $iv = 'i'; private static $sv = 's'; }
$w = new SnoPriv;
try { var_dump($w->iv); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump($w->sv); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== a class with the matching magic accessor is silent ==\n";
class SnoGet { public static $s = 'st'; public function __get($n) { return "G"; } }
class SnoUnset { public static $s = 'st'; public function __unset($n) { echo "  __unset(", $n, ")\n"; } }
$g = new SnoGet; $u = new SnoUnset;
var_dump($g->s);
unset($u->s);

echo "== and the class-level access still works ==\n";
SnoBase::$s = 'moved';
var_dump(SnoBase::$s, SnoBase::K);
echo "end\n";
?>
--EXPECTF--
== the raw surfaces list only the object's own properties ==
array(2) {
  ["p"]=>
  int(1)
  ["%A*%Aq"]=>
  int(2)
}
array(1) {
  ["p"]=>
  int(1)
}
object(SnoBase)#1 (2) {
  ["p"]=>
  int(1)
  ["q":protected]=>
  int(2)
}
SnoBase Object
(
    [p] => 1
    [q:protected] => 2
)

string(7) "{"p":1}"
== foreach ==
  p => 1
== reading it through the instance ==
%ANotice:  Accessing static property SnoBase::$s as non static in %s on line %d
%AWarning:  Undefined property: SnoBase::$s in %s on line %d
NULL
== isset()/empty() are silent ==
bool(false)
bool(true)
== writing through the instance leaves the CLASS static alone ==
%ANotice:  Accessing static property SnoBase::$s as non static in %s on line %d
string(2) "st"
== a by-reference argument cannot reach the class slot either ==
%ANotice:  Accessing static property SnoRef::$s as non static in %s on line %d
string(2) "st"
== a NON-PUBLIC static is an access Error, as for any private member ==
Error: Cannot access private property SnoPriv::$iv
Error: Cannot access private property SnoPriv::$sv
== a class with the matching magic accessor is silent ==
string(1) "G"
  __unset(s)
== and the class-level access still works ==
string(5) "moved"
string(2) "kk"
end
--CLEAN--
<?php
unset($o, $r, $w, $g, $u);
