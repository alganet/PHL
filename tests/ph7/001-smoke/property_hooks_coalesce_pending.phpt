--TEST--
Property hooks / magic ??= pending-set robustness: nested coalesces, throws in the RHS, scalar-as-array errors, increment type errors
--FILE--
<?php
// an inner ??= in the RHS must not disturb the outer pending set
class HcA {
    public ?int $p = null {
        get => $this->p;
        set { echo "SET"; $this->p = $value; }
    }
}
function hcF() { $y = 1; $y ??= 2; return 7; }
$hcA = new HcA();
$hcA->p ??= hcF();
echo "|", $hcA->p, "\n";
// a throw in the RHS discards the pending set; a later unrelated store is unaffected
class HcD { public $x = 0; }
function hcThrower() { throw new Exception("t"); }
$hcB = new HcA();
$hcD = new HcD();
try { $hcB->p ??= hcThrower(); } catch (Exception $e) { echo "caught|"; }
$hcD->x = 1;
echo "d=", $hcD->x, " p=", var_export($hcB->p, true), "\n";
// nested hooked coalesce assigns inner-then-outer
class HcN {
    public ?int $p = null { get => $this->p; set { echo "P(", $value, ")"; $this->p = $value; } }
    public ?int $q = null { get => $this->q; set { echo "Q(", $value, ")"; $this->q = $value; } }
}
$hcN = new HcN();
$hcN->p ??= ($hcN->q ??= 3);
echo "|p=", $hcN->p, " q=", $hcN->q, "\n";
// a member store inside the RHS must not consume the outer pending set
class HcH {
    public $store = null;
    public ?int $x = null { get => $this->x; set { echo "HOOK(", $value, ")"; $this->store = $value; $this->x = $value; } }
    public int $plain = 0;
}
class HcW { public $v = 0; }
$hcH = new HcH();
$hcW = new HcW();
function hcG() { global $hcW; $hcW->v = 123; return 5; }
$hcH->x ??= hcG();
echo "|w=", $hcW->v, " store=", $hcH->store, "\n";
// magic ??= with a throwing RHS: __set never runs, later stores unaffected
class HcM {
    private $d = [];
    public function __get($n) { echo "G"; return $this->d[$n] ?? null; }
    public function __set($n, $v) { echo "S"; $this->d[$n] = $v; }
}
$hcM = new HcM();
try { $hcM->p ??= hcThrower(); } catch (Exception $e) { echo "caught|"; }
$hcD->x = 2;
echo "d=", $hcD->x, "\n";
// scalar bases refuse subscript writes (php 8): int/float/true error, null/false convert
$hcI = 5;
try { $hcI[0]++; } catch (Error $e) { echo $e->getMessage(), "; "; }
var_export($hcI); echo "\n";
$hcJ = 1.5;
try { $hcJ[0][0] = "x"; } catch (Error $e) { echo $e->getMessage(), "; "; }
var_export($hcJ); echo "\n";
$hcK = true;
try { $hcK[2] ??= 9; } catch (Error $e) { echo $e->getMessage(), "; "; }
var_export($hcK); echo "\n";
$hcL = null;
$hcL[0][1] = "ok";
echo $hcL[0][1], "\n";
// ++/-- on an array-valued hooked property: php's TypeError, set hook never runs
class HcArr {
    public array $m = [] { get => $this->m; set { echo "SETM"; $this->m = $value; } }
}
$hcArr = new HcArr();
try { $hcArr->m++; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { $hcArr->m--; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
SET|7
caught|d=1 p=NULL
Q(3)P(3)|p=3 q=3
HOOK(5)|w=123 store=5
Gcaught|d=2
Cannot use a scalar value as an array; 5
Cannot use a scalar value as an array; 1.5
Cannot use a scalar value as an array; true
ok
Cannot increment array
Cannot decrement array
