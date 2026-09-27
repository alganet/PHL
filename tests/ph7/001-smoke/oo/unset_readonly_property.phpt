--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset() of a readonly property is refused, and a typed one keeps its declaration
--DESCRIPTION--
`unset($o->p)` on a READONLY property is an Error in php, and it has to be: the
write-once latch is the property's initialization state, so destroying it would
re-arm the latch and let a readonly value be replaced in two statements.

php refuses it in three shapes and allows it in one:

  * an INITIALIZED one is refused from every scope, its own included --
    `Cannot unset readonly property C::$p`;
  * an UNINITIALIZED one is a write-shaped act and takes the set-visibility
    rules: allowed from the declaring class or a subclass, refused elsewhere;
  * that refusal is worded two ways -- an explicit `private(set)` gets the
    ordinary asymmetric-visibility sentence with no "readonly" in it, and
    everything else gets readonly's own implicit `protected(set) readonly`.

Under it, the same opcode's other rule: unsetting a TYPED property leaves the
DECLARATION standing. php marks it uninitialized rather than removing it, so
var_dump still names it as `uninitialized(T)` while the seven other
presentation surfaces leave it out, a read is "must not be accessed before
initialization", and a later write lands back in the slot it declared.
--FILE--
<?php
class UnsetRoBase
{
    public readonly int $init;
    public readonly int $blank;
    public private(set) readonly int $priv;
    public protected(set) readonly int $prot;

    public function __construct() { $this->init = 1; }
    public function unsetInit() { unset($this->init); }
    public function unsetBlank() { unset($this->blank); }
    public function unsetPriv() { unset($this->priv); }
}
class UnsetRoChild extends UnsetRoBase
{
    public function unsetBlankFromChild() { unset($this->blank); }
    public function unsetPrivFromChild() { unset($this->priv); }
}

/* var_dump names the object HANDLE, and that number counts every object made
   before this test in the shared interpreter -- scrub it. */
function unsetRoDump($v)
{
    ob_start();
    var_dump($v);
    echo preg_replace('/#\d+/', '#N', ob_get_clean());
}

function unsetRoTry($label, callable $f)
{
    try { $f(); printf("%-34s => ok\n", $label); }
    catch (Throwable $e) { printf("%-34s => %s: %s\n", $label, get_class($e), $e->getMessage()); }
}

$o = new UnsetRoBase();
unsetRoTry('initialized, outside', function () use ($o) { unset($o->init); });
unsetRoTry('initialized, inside', function () use ($o) { $o->unsetInit(); });
unsetRoTry('uninitialized, outside', function () use ($o) { unset($o->blank); });
unsetRoTry('uninitialized, inside', function () use ($o) { $o->unsetBlank(); });
unsetRoTry('private(set), outside', function () use ($o) { unset($o->priv); });
unsetRoTry('private(set), inside', function () use ($o) { $o->unsetPriv(); });
unsetRoTry('protected(set), outside', function () use ($o) { unset($o->prot); });

$c = new UnsetRoChild();
unsetRoTry('uninitialized, from a subclass', function () use ($c) { $c->unsetBlankFromChild(); });
unsetRoTry('private(set), from a subclass', function () use ($c) { $c->unsetPrivFromChild(); });

echo "== and the value is still there ==\n";
var_dump($o->init, isset($o->init), isset($o->blank));
// the one unset that WAS allowed left the property uninitialized, not gone
var_dump(property_exists($o, 'blank'));
try { var_dump($o->blank); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// ... and a readonly property may still take its one write afterwards
$p = new UnsetRoBase();
$p->unsetBlank();
(function () { $this->blank = 7; })->call($p);
var_dump($p->blank);
unsetRoTry('written, then unset from inside', function () use ($p) { $p->unsetBlank(); });

echo "== a typed property keeps its declaration ==\n";
class UnsetRoTyped
{
    public int $a = 1;
    public $b = 2;
    public ?string $c = 'x';
}
$t = new UnsetRoTyped();
unset($t->a, $t->b, $t->c);
unsetRoDump($t);
print_r($t);
echo "\n";
var_dump(get_object_vars($t), json_encode($t), (array) $t, isset($t->a), isset($t->b));
foreach ($t as $k => $v) { echo "iter $k\n"; }
try { var_dump($t->a); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// writing it back clears the state, in the slot the class declared
$t->a = 5;
var_dump($t->a);
unsetRoDump($t);
// and the declared type is still enforced there
try { $t->c = []; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// a clone carries the uninitialized state
$u = clone $t;
unsetRoDump($u);
--EXPECT--
initialized, outside               => Error: Cannot unset readonly property UnsetRoBase::$init
initialized, inside                => Error: Cannot unset readonly property UnsetRoBase::$init
uninitialized, outside             => Error: Cannot unset protected(set) readonly property UnsetRoBase::$blank from global scope
uninitialized, inside              => ok
private(set), outside              => Error: Cannot unset private(set) property UnsetRoBase::$priv from global scope
private(set), inside               => ok
protected(set), outside            => Error: Cannot unset protected(set) readonly property UnsetRoBase::$prot from global scope
uninitialized, from a subclass     => ok
private(set), from a subclass      => Error: Cannot unset private(set) property UnsetRoBase::$priv from scope UnsetRoChild
== and the value is still there ==
int(1)
bool(true)
bool(false)
bool(true)
Error: Typed property UnsetRoBase::$blank must not be accessed before initialization
int(7)
written, then unset from inside    => Error: Cannot unset readonly property UnsetRoBase::$blank
== a typed property keeps its declaration ==
object(UnsetRoTyped)#N (0) {
  ["a"]=>
  uninitialized(int)
  ["c"]=>
  uninitialized(?string)
}
UnsetRoTyped Object
(
)

array(0) {
}
string(2) "{}"
array(0) {
}
bool(false)
bool(false)
Error: Typed property UnsetRoTyped::$a must not be accessed before initialization
int(5)
object(UnsetRoTyped)#N (1) {
  ["a"]=>
  int(5)
  ["c"]=>
  uninitialized(?string)
}
TypeError: Cannot assign array to property UnsetRoTyped::$c of type ?string
object(UnsetRoTyped)#N (1) {
  ["a"]=>
  int(5)
  ["c"]=>
  uninitialized(?string)
}
