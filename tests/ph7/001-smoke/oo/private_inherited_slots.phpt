--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A base's private property is a SLOT of its own on every object below it
--DESCRIPTION--
php files a private property in the object under its mangled storage name,
"\0DeclaringClass\0name", so a base and a child that both declare `$q` have TWO
slots on one instance and each class's code reaches only its own. This engine kept
one, so a base method reading its own private got the CHILD's value -- a wrong
answer with nothing to announce it, in the shape that occurs whenever a framework
base class and a subclass happen to pick the same private name.
--FILE--
<?php
class PrivSlotBase {
    private $q = 1;
    public function baseRead()      { return $this->q; }
    public function baseWrite($v)   { $this->q = $v; }
    public function baseIsset()     { return isset($this->q); }
    public function baseUnset()     { unset($this->q); }
    public function baseVars()      { return array_keys(get_object_vars($this)); }
    public function baseWalk()      { $r = []; foreach ($this as $k => $v) { $r[] = "$k=$v"; } return $r; }
}
class PrivSlotChild extends PrivSlotBase {
    private $q = 2;
    public function childRead()     { return $this->q; }
    public function childWrite($v)  { $this->q = $v; }
}
class PrivSlotGrand extends PrivSlotChild {}

function priv_slot_mangled($s) { return str_replace("\0", '@', $s); }

$o = new PrivSlotChild();
var_dump($o->baseRead(), $o->childRead());
$o->baseWrite(10);
$o->childWrite(20);
var_dump($o->baseRead(), $o->childRead());
var_dump($o);
echo priv_slot_mangled(serialize($o)), "\n";
var_dump(array_map('priv_slot_mangled', array_keys((array) $o)));
var_dump(array_map('priv_slot_mangled', array_keys(get_mangled_object_vars($o))));

/* The by-NAME surfaces present each plain name once: base-first, so a base method
 * iterating a child instance sees its own $q and never the child's. */
var_dump($o->baseVars(), $o->baseWalk());
var_dump(get_object_vars($o));

/* Both slots survive a clone and a serialize round trip. */
$c = clone $o;
var_dump($c->baseRead(), $c->childRead());
$u = unserialize(serialize($o));
var_dump($u->baseRead(), $u->childRead());

/* unset() takes only the slot the executing scope means; a re-write puts it back. */
$o->baseUnset();
var_dump($o->baseIsset(), $o->childRead());
echo priv_slot_mangled(serialize($o)), "\n";
$o->baseWrite(30);
var_dump($o->baseRead(), $o->childRead());

/* A grandchild carries both, still one per declaring class. */
$g = new PrivSlotGrand();
var_dump($g->baseRead(), $g->childRead());
echo priv_slot_mangled(serialize($g)), "\n";

/* Reflection reads and writes the DECLARING class's slot. */
$rp = new ReflectionProperty('PrivSlotBase', 'q');
var_dump($rp->getValue($g));
$rp->setValue($g, 40);
var_dump($g->baseRead(), $g->childRead());

/* Neither private is part of any other class's surface. */
var_dump(property_exists('PrivSlotGrand', 'q'));
var_dump(array_map(fn($p) => $p->getDeclaringClass()->getName() . '::' . $p->getName(),
    (new ReflectionClass('PrivSlotGrand'))->getProperties()));
?>
--EXPECTF--
int(1)
int(2)
int(10)
int(20)
object(PrivSlotChild)#%d (2) {
  ["q":"PrivSlotBase":private]=>
  int(10)
  ["q":"PrivSlotChild":private]=>
  int(20)
}
O:13:"PrivSlotChild":2:{s:15:"@PrivSlotBase@q";i:10;s:16:"@PrivSlotChild@q";i:20;}
array(2) {
  [0]=>
  string(15) "@PrivSlotBase@q"
  [1]=>
  string(16) "@PrivSlotChild@q"
}
array(2) {
  [0]=>
  string(15) "@PrivSlotBase@q"
  [1]=>
  string(16) "@PrivSlotChild@q"
}
array(1) {
  [0]=>
  string(1) "q"
}
array(1) {
  [0]=>
  string(4) "q=10"
}
array(0) {
}
int(10)
int(20)
int(10)
int(20)
bool(false)
int(20)
O:13:"PrivSlotChild":1:{s:16:"@PrivSlotChild@q";i:20;}
int(30)
int(20)
int(1)
int(2)
O:13:"PrivSlotGrand":2:{s:15:"@PrivSlotBase@q";i:1;s:16:"@PrivSlotChild@q";i:2;}
int(1)
int(40)
int(2)
bool(false)
array(0) {
}
--CLEAN--
<?php
unset($o, $c, $u, $g, $rp);
