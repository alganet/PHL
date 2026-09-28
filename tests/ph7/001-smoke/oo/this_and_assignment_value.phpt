--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What an assignment expression IS: its value, its lvalue-ness, and `$this`
--DESCRIPTION--
Three rules php makes about an assignment expression, each wrong here in its own
way. A store through ArrayAccess is an EXPRESSION and its value is what was
assigned -- this engine popped it and left the expression reading a stack slot it
no longer owned. `$this` may not be an assignment TARGET, but it is a perfectly
good reference SOURCE (php compiles a `=&` source in write context without ever
asking zend_ensure_writable_variable, which is the function that holds the $this
rule), and the bind degrades to a copy of the object handle. And a reference
ASSIGNMENT hands back the reference it made, so it can be passed on to a by-ref
parameter where a plain `$q = $p` cannot.
--FILE--
<?php
/* A store through ArrayAccess is an expression whose value is what was ASSIGNED
 * -- not what the setter chose to keep, and not what offsetGet would answer. */
class TavA implements ArrayAccess {
    public array $d = [];
    public function offsetExists(mixed $o): bool { return isset($this->d[$o]); }
    public function offsetGet(mixed $o): mixed { return 111; }
    public function offsetSet(mixed $o, mixed $v): void { $this->d[$o] = $v * 2; }
    public function offsetUnset(mixed $o): void { unset($this->d[$o]); }
}
$tavA = new TavA;
var_dump($tavA['x'] = 7, $tavA->d['x']);
$tavB = ($tavA['y'] = 5);
var_dump($tavB);
echo ($tavA['z'] = 3), "\n";

/* The same through ArrayObject, its subclass, and the keyless append. */
class TavSub extends ArrayObject {}
$tavC = new TavSub;
var_dump($tavC['a'] = 42, $tavC[] = 3, $tavC[0]);
$tavE = new ArrayObject();
var_dump($tavE['k'] = 9);
$tavD = new SplFixedArray(2);
var_dump($tavD[0] = 5);
class TavRet extends ArrayObject { public function f(string $k): mixed { return $this[$k] = 42; } }
var_dump((new TavRet)->f('a'));

/* `$this` is a reference SOURCE. The bind is a copy of the handle: the same
 * object (so a property write through it is visible), but its own slot -- a
 * later `$t = 5` leaves the receiver an object. */
class TavThis {
    public $v = 1;
    public $p;
    public static $s;
    public function bind()    { $t =& $this; return get_class($t); }
    public function write()   { $t =& $this; $t->v = 9; return $this->v; }
    public function rebind()  { $t =& $this; $t = 5; return [is_object($this), $this->v]; }
    public function toArray() { $a = []; $a[] =& $this; return get_class($a[0]); }
    public function toProp()  { $this->p =& $this; return get_class($this->p); }
    public function toStatic(){ self::$s =& $this; return get_class(self::$s); }
    public function chain()   { $t =& $this; $u =& $t; $t = 7; return [is_object($this), $u]; }
}
var_dump((new TavThis)->bind(), (new TavThis)->write());
var_dump((new TavThis)->rebind());
var_dump((new TavThis)->toArray(), (new TavThis)->toProp(), (new TavThis)->toStatic());
var_dump((new TavThis)->chain());

/* `$this` is refused as an assignment TARGET only. A read-modify-write is not an
 * assignment: php compiles it and fails at RUN time on the operand types. */
class TavRmw {
    public function inc()  { try { $this++; } catch (Throwable $e) { return $e->getMessage(); } }
    public function add()  { try { $this += 1; } catch (Throwable $e) { return $e->getMessage(); } }
    public function cat()  { try { $this .= 'x'; } catch (Throwable $e) { return $e->getMessage(); } }
}
var_dump((new TavRmw)->inc(), (new TavRmw)->add(), (new TavRmw)->cat());

/* A frame with no receiver answers a READ of `$this` with php's Error -- even
 * under `??`, which swallows an ordinary undefined variable. isset()/empty() are
 * php's one exemption and answer in silence; so is compact(). */
function tavFree() {
    try { var_dump($this); } catch (Error $e) { echo $e->getMessage(), "\n"; }
    try { var_dump($this->x); } catch (Error $e) { echo $e->getMessage(), "\n"; }
    try { var_dump($this ?? 'd'); } catch (Error $e) { echo $e->getMessage(), "\n"; }
    try { $t =& $this; } catch (Error $e) { echo $e->getMessage(), "\n"; }
    var_dump(isset($this), empty($this), compact('this'));
}
tavFree();
class TavStatic { public static function f() { try { var_dump($this); } catch (Error $e) { echo $e->getMessage(), "\n"; } } }
TavStatic::f();
$tavClosure = static function () { try { var_dump($this); } catch (Error $e) { echo $e->getMessage(), "\n"; } };
$tavClosure();

/* A reference ASSIGNMENT is passable by reference; a plain one is not. */
function tavTake(&$z) { $z = 8; return $z; }
$tavP = 1;
var_dump(tavTake($tavQ = &$tavP), $tavP, $tavQ);
$tavR = 'a';
tavTake($tavS = &$tavR);
var_dump($tavR, $tavS);
?>
--EXPECT--
int(7)
int(14)
int(5)
3
int(42)
int(3)
int(3)
int(9)
int(5)
int(42)
string(7) "TavThis"
int(9)
array(2) {
  [0]=>
  bool(true)
  [1]=>
  int(1)
}
string(7) "TavThis"
string(7) "TavThis"
string(7) "TavThis"
array(2) {
  [0]=>
  bool(true)
  [1]=>
  int(7)
}
string(23) "Cannot increment TavRmw"
string(39) "Unsupported operand types: TavRmw + int"
string(55) "Object of class TavRmw could not be converted to string"
Using $this when not in object context
Using $this when not in object context
Using $this when not in object context
Using $this when not in object context
bool(false)
bool(true)
array(0) {
}
Using $this when not in object context
Using $this when not in object context
int(8)
int(8)
int(8)
int(8)
int(8)
--CLEAN--
<?php
