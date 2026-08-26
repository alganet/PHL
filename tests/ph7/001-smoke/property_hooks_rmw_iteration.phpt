--TEST--
Property hooks (PHP 8.4) slice 2: read-modify-write dispatch, ??=, subscript errors, iteration/get_object_vars/json/var_export get-dispatch
--FILE--
<?php
// ++ / -- / compound assigns dispatch get then set
class HrA {
    public int $x = 1 {
        get => $this->x * 10;
        set { $this->x = $value + 1; }
    }
}
$hrA = new HrA();
$hrA->x++;
echo $hrA->x, "\n";                       // get(1)=10, ++ -> 11, set(11) -> 12, get -> 120
$hrB = new HrA();
echo "post=", $hrB->x++, " mid=", $hrB->x, "\n";
$hrC = new HrA();
echo "pre=", ++$hrC->x, "\n";
$hrD = new HrA();
$hrRes = ($hrD->x += 5);
echo "r=", $hrRes, " final=", $hrD->x, "\n";
// .= via get/set
class HrS {
    public string $s = "a" {
        get => strtoupper($this->s);
        set { $this->s = $value . "!"; }
    }
}
$hrS = new HrS();
$hrS->s .= "b";
echo $hrS->s, "\n";                       // get "A", ."b" -> "Ab", set -> backing "Ab!", get -> "AB!"
// RMW on a get-only hook: get runs, then php's read-only Error
class HrG { public int $g { get { echo "G"; return 5; } } }
$hrG = new HrG();
try { $hrG->g++; } catch (Error $e) { echo "|", $e->getMessage(), "\n"; }
// RMW on a set-only hook reads the raw backing store
class HrO { public int $o = 1 { set { $this->o = $value * 100; } } }
$hrO = new HrO();
$hrO->o++;
echo $hrO->o, "\n";
// subscript writes and subscript RMW: php's Indirect-modification Error
class HrM { public array $m = [1] { get => $this->m; set { $this->m = $value; } } }
$hrM = new HrM();
try { $hrM->m[] = 9; } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { $hrM->m[0]++; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// ??= dispatches get for the test and set for the assign
class HrN {
    public ?int $n = null {
        get => $this->n;
        set { $this->n = $value + 1; }
    }
}
$hrN = new HrN();
$hrN->n ??= 5;
echo $hrN->n, "\n";
$hrN2 = new HrN();
$hrN2->n = 0;      // set(0) -> backing 1
$hrN2->n ??= 7;    // get -> 1, non-null: assign skipped
echo $hrN2->n, "\n";
// a throw during the modify op discards the write (set hook never runs)
class HrT { function __toString(): string { throw new Exception("boom"); } }
class HrU {
    public string $u = "a" {
        get => strtoupper($this->u);
        set { echo "SET"; $this->u = $value; }
    }
}
$hrU = new HrU();
try { $hrU->u .= new HrT(); } catch (Exception $e) { echo "caught "; }
echo $hrU->u, "\n";
// inside a hook body $this->prop is raw for BOTH directions
class HrX {
    public int $c = 1 {
        get { $this->c = 99; return $this->c; }   // raw write, raw read
        set { $this->c = $value + 1; }
    }
}
$hrX = new HrX();
echo $hrX->c, "\n";
class HrY {
    public int $d = 1 {
        get => $this->d * 10;
        set { $this->d = $value + $this->d; }     // raw read inside set
    }
}
$hrY = new HrY();
$hrY->d = 5;
echo $hrY->d, "\n";
// `new` is allowed inside hook bodies of a property WITH a default
class HrV { public int $vv = 9; }
class HrW { public int $w = 1 { get { $o = new HrV(); return $this->w + $o->vv; } } }
echo (new HrW())->w, "\n";
// iteration surfaces dispatch get (virtual props included); by-ref foreach errors
class HrI {
    public int $virt { get => 42; }
    public int $backed = 1 { get => $this->backed * 10; set { $this->backed = $value + 1; } }
    public int $plain = 7;
    private int $priv = 5 { get => $this->priv * 2; }
}
$hrI = new HrI();
foreach ($hrI as $k => $v) { echo $k, "=", $v, ";"; }
echo "\n";
echo json_encode(get_object_vars($hrI)), "\n";
echo json_encode($hrI), "\n";
var_export($hrI);
echo "\n";
try { foreach ($hrI as $k => &$v) {} } catch (Error $e) { echo $e->getMessage(), "\n"; }
// get_object_vars from inside the class sees private hooked values through get
class HrP {
    public int $v { get => 7; }
    private int $q = 3 { get => $this->q * 5; }
    function all(): array { return get_object_vars($this); }
}
echo json_encode((new HrP())->all()), "\n";
// magic ??= : __isset consulted first, then __set (no __get on a miss)
class HrQ {
    private $d = [];
    public function __isset($n) { echo "I"; return isset($this->d[$n]); }
    public function __get($n) { echo "G"; return $this->d[$n] ?? null; }
    public function __set($n, $v) { echo "S"; $this->d[$n] = $v; }
}
$hrQ = new HrQ();
$hrQ->p ??= 5;
echo "|", $hrQ->p, "\n";
$hrQ->p ??= 9;
echo "|", $hrQ->p, "\n";
?>
--EXPECT--
120
post=10 mid=120
pre=11
r=15 final=160
AB!
G|Property HrG::$g is read-only
200
Indirect modification of HrM::$m is not allowed
Indirect modification of HrM::$m is not allowed
6
1
caught A
99
60
10
virt=42;backed=10;plain=7;
{"virt":42,"backed":10,"plain":7}
{"virt":42,"backed":10,"plain":7}
\HrI::__set_state(array(
   'virt' => 42,
   'backed' => 10,
   'plain' => 7,
   'priv' => 10,
))
Cannot create reference to property HrI::$virt
{"v":7,"q":15}
IS|G5
IG|G5
