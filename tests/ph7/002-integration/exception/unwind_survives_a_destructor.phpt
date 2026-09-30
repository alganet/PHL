--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A destructor runs inside somebody else's unwind: it neither exists after a failed `new` nor steals its landing
--DESCRIPTION--
Two rules, one situation. php marks an object whose CONSTRUCTOR raised
(zend_object_store_ctor_failed sets the very bit that records "the destructor has been
reached for"), so its __destruct never runs -- not at the `new`, and not later either, since
the mark rides the object wherever the constructor stored `$this` before it threw. A property
DEFAULT that throws is the same answer for a different reason: php evaluates the defaults
before the object exists, so there is nothing to destruct. PHL built the instance first and
ran the destructor on both.

And a destructor that DOES run is user code executing in the middle of somebody else's
control flow -- the release that reaches it is a frame teardown on an unwind already
carrying a throw. php hides the in-flight exception across the body for exactly that reason;
PHL's equivalent in-flight state is the in-place-catch resume record, and a destructor body
with a try/catch of its own wrote a record of its own over it when its catch finished. The
outer throw then never landed and the script simply ENDED (monolog's `Handler::__destruct`
is `try { $this->close(); } catch (Throwable) {}`, and its suite died on it).

The ORDER of a swallow relative to the catch body is not asserted: php destroys a frame's
locals as it unwinds, this engine after the in-place catch body (PLAN.md's in-place-catch
ordering row), so the counter is only read where both have finished the same teardown.
--FILE--
<?php
echo "== a constructor that throws leaves no object to destruct ==\n";
class A {
    public function __construct(bool $fail) { echo "A::ctor\n"; if ($fail) { throw new RuntimeException('boom'); } }
    public function __destruct() { echo "A::dtor\n"; }
}
try { new A(true); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }
echo "== a good one still destructs ==\n";
$a = new A(false);
unset($a);

echo "== the mark follows the object the constructor stored ==\n";
$GLOBALS['kept'] = null;
class B {
    public function __construct() { $GLOBALS['kept'] = $this; throw new LogicException('kept'); }
    public function __destruct() { echo "B::dtor\n"; }
}
try { new B(); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }
var_dump($GLOBALS['kept'] instanceof B);
$GLOBALS['kept'] = null;
echo "still nothing\n";

echo "== a subclass that throws after parent::__construct ==\n";
class C { public function __construct() { echo "C::ctor\n"; } public function __destruct() { echo "C::dtor\n"; } }
class D extends C { public function __construct() { parent::__construct(); throw new DomainException('d'); } }
try { new D(); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }

echo "== a property DEFAULT that throws is the same rule ==\n";
class E { public array $t = ['k' => UNDEF_HERE]; public function __destruct() { echo "E::dtor\n"; } }
try { new E(); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }

echo "== Reflection's newInstance answers the same ==\n";
try { (new ReflectionClass('A'))->newInstance(true); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }
try { (new ReflectionClass('A'))->newInstanceArgs([true]); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }

/* A destructor is user code that runs in the MIDDLE of somebody else's unwind. Its own
 * try/catch must not consume the landing the outer throw is still owed. The counter is
 * read only at points both engines have reached the same teardown (php destroys a frame's
 * locals as it unwinds, this engine after the in-place catch body -- see PLAN.md's
 * in-place-catch ordering row), so the ORDER of the swallow is not asserted here. */
class Guard {
    public static int $swallowed = 0;
    public function close(): void { throw new RuntimeException('close failed'); }
    public function __destruct() { try { $this->close(); } catch (Throwable $e) { Guard::$swallowed++; } }
}
echo "== a destructor's own try/catch does not steal the unwind ==\n";
function inner() { $g = new Guard(); throw new UnexpectedValueException('deep'); }
function middle() { return inner(); }
try {
    middle();
    echo "NOT REACHED\n";
} catch (Throwable $e) {
    echo "outer caught ", $e->getMessage(), "\n";
}
echo "after the outer catch, swallowed=", Guard::$swallowed, "\n";

echo "== the same, two guards per frame and twice over ==\n";
function inner2() { $g = new Guard(); $h = new Guard(); throw new UnexpectedValueException('deep2'); }
for ($i = 0; $i < 2; $i++) {
    try { inner2(); } catch (Throwable $e) { echo "loop$i caught ", $e->getMessage(), "\n"; }
}
echo "still running, swallowed=", Guard::$swallowed, "\n";

echo "== the throw two frames below the catching try ==\n";
function deepC() { $g = new Guard(); throw new OutOfRangeException('three'); }
function deepB() { return deepC(); }
function deepA() { return deepB(); }
try { deepA(); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }
echo "swallowed=", Guard::$swallowed, "\n";

echo "== a destructor running on a normal return arms nothing ==\n";
function retpath() { $g = new Guard(); return 'value'; }
echo retpath(), "\n";
echo "swallowed=", Guard::$swallowed, "\n";

echo "== an inner try/catch inside the unwound frame still works ==\n";
function inner3() {
    $g = new Guard();
    try { throw new RangeException('local'); } catch (RangeException $e) { echo "local caught\n"; }
    throw new UnexpectedValueException('escapes');
}
try { inner3(); } catch (Throwable $e) { echo "caught ", $e->getMessage(), "\n"; }
echo "swallowed=", Guard::$swallowed, "\n";
echo "done\n";
?>
--EXPECT--
== a constructor that throws leaves no object to destruct ==
A::ctor
caught boom
== a good one still destructs ==
A::ctor
A::dtor
== the mark follows the object the constructor stored ==
caught kept
bool(true)
still nothing
== a subclass that throws after parent::__construct ==
C::ctor
caught d
== a property DEFAULT that throws is the same rule ==
caught Undefined constant "UNDEF_HERE"
== Reflection's newInstance answers the same ==
A::ctor
caught boom
A::ctor
caught boom
== a destructor's own try/catch does not steal the unwind ==
outer caught deep
after the outer catch, swallowed=1
== the same, two guards per frame and twice over ==
loop0 caught deep2
loop1 caught deep2
still running, swallowed=5
== the throw two frames below the catching try ==
caught three
swallowed=6
== a destructor running on a normal return arms nothing ==
value
swallowed=7
== an inner try/catch inside the unwound frame still works ==
local caught
caught escapes
swallowed=8
done
