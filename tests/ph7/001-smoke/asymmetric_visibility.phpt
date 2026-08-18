--TEST--
Asymmetric visibility (PHP 8.4): private(set)/protected(set) on declared, promoted, and static properties
--FILE--
<?php
class AsvA {
    public private(set) int $x = 1;
    function bump() { $this->x++; }
}
$asvA = new AsvA();
echo $asvA->x;
$asvA->bump();
echo $asvA->x, "\n";
try { $asvA->x = 5; } catch (Error $e) { echo get_class($e), ":", $e->getMessage(), "\n"; }
try { $asvA->x++; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// protected(set): subclass may write, outside may not
class AsvB { public protected(set) int $y = 1; function w($v) { $this->y = $v; } }
class AsvC extends AsvB { function w2($v) { $this->y = $v; } }
$asvC = new AsvC();
$asvC->w(5); echo $asvC->y;
$asvC->w2(9); echo $asvC->y, "\n";
try { $asvC->y = 2; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// private(set): even a subclass may not write
class AsvD extends AsvA { function w3() { $this->x = 7; } }
$asvD = new AsvD();
try { $asvD->w3(); } catch (Error $e) { echo $e->getMessage(), "\n"; }
// standalone private(set) implies a public read side
class AsvE { private(set) int $z = 3; function set($v) { $this->z = $v; } }
$asvE = new AsvE();
echo $asvE->z;
$asvE->set(4);
echo $asvE->z, "\n";
try { $asvE->z = 9; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// readonly + private(set): a write after init reports the readonly error
class AsvF { public private(set) readonly int $r; function __construct() { $this->r = 1; } }
$asvF = new AsvF();
try { $asvF->r = 2; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// promoted asymmetric property
class AsvG {
    function __construct(public private(set) int $p = 5) {}
    function inc() { $this->p++; }
}
$asvG = new AsvG();
echo $asvG->p; $asvG->inc(); echo $asvG->p, "\n";
try { $asvG->p = 8; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// leading set-visibility on a promoted param
class AsvH { function __construct(private(set) int $q = 6) {} }
echo (new AsvH())->q, "\n";
// explicit public(set) behaves like a plain public property
class AsvI { public public(set) int $w = 1; }
$asvI = new AsvI();
$asvI->w = 5;
echo $asvI->w, "\n";
// static property with asymmetric visibility
class AsvJ { public static private(set) int $s = 1; static function set($v) { self::$s = $v; } }
echo AsvJ::$s;
AsvJ::set(3);
echo AsvJ::$s, "\n";
try { AsvJ::$s = 9; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// reflection surface
$asvP = new ReflectionProperty("AsvA", "x");
$asvQ = new ReflectionProperty("AsvB", "y");
$asvR = new ReflectionProperty("AsvI", "w");
echo (int) $asvP->isPrivateSet(), (int) $asvP->isProtectedSet(),
     (int) $asvQ->isPrivateSet(), (int) $asvQ->isProtectedSet(),
     (int) $asvR->isPrivateSet(), (int) $asvR->isProtectedSet(), "\n";
--EXPECT--
12
Error:Cannot modify private(set) property AsvA::$x from global scope
Cannot modify private(set) property AsvA::$x from global scope
59
Cannot modify protected(set) property AsvB::$y from global scope
Cannot modify private(set) property AsvA::$x from scope AsvD
34
Cannot modify private(set) property AsvE::$z from global scope
Cannot modify readonly property AsvF::$r
56
Cannot modify private(set) property AsvG::$p from global scope
6
5
13
Cannot modify private(set) property AsvJ::$s from global scope
100100
