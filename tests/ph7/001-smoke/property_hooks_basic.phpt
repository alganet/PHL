--TEST--
Property hooks (PHP 8.4): get/set hooks, backing-store access, defaults, inheritance, errors
--FILE--
<?php
// virtual computed property
class PhkA {
    public int $n = 3;
    public string $virt { get => "V" . $this->n; }
}
$phkA = new PhkA();
echo $phkA->virt, "\n";
echo isset($phkA->virt) ? "T" : "F", empty($phkA->virt) ? "E" : "N", "\n";
// backed property with both hooks (block bodies); hooks see the raw backing store
class PhkB {
    public string $b { get { return strtoupper($this->b); } set(string $v) { $this->b = trim($v); } }
}
$phkB = new PhkB();
$phkB->b = "  hi  ";
echo $phkB->b, "|", strlen($phkB->b), "\n";
// set => expr shorthand assigns to the backing store
class PhkC { public int $s { set => $value * 2; } }
$phkC = new PhkC();
$phkC->s = 4;
echo $phkC->s, "\n";
// explicit set(Type $v) writing the backing store
class PhkD { public int $t { set(int $v) { $this->t = $v + 10; } } }
$phkD = new PhkD();
$phkD->t = 4;
echo $phkD->t, "\n";
// a get-only property rejects writes with php's catchable Error
class PhkE { public string $v { get => "x"; } }
$phkE = new PhkE();
try { $phkE->v = "y"; } catch (Error $e) { echo get_class($e), ":", $e->getMessage(), "\n"; }
echo $phkE->v, "\n";
// children inherit hooks; a child may override them
class PhkF { public int $h = 0; public int $tw { get => $this->h * 2; set(int $v) { $this->h = $v; } } }
class PhkG extends PhkF {}
$phkG = new PhkG();
$phkG->tw = 21;
echo $phkG->tw, ",", $phkG->h, "\n";
class PhkH extends PhkF { public int $tw { get => $this->h * 3; set(int $v) { $this->h = $v; } } }
$phkH = new PhkH();
$phkH->tw = 10;
echo $phkH->tw, ",", (new PhkF())->h, "\n";
// hooks are ordinary code: call methods, use other properties
class PhkI {
    private array $parts = [];
    public string $joined { get => implode("-", $this->parts); set(string $v) { $this->parts = explode("-", $v); } }
}
$phkI = new PhkI();
$phkI->joined = "a-b-c";
echo $phkI->joined, "\n";
// a backing default value coexists with hooks
class PhkJ { public string $w = "init" { get => "[" . $this->w . "]"; set(string $v) { $this->w = $v; } } }
$phkJ = new PhkJ();
echo $phkJ->w, "\n";
$phkJ->w = "next";
echo $phkJ->w, "\n";
// exceptions thrown inside hooks are catchable at the access site
class PhkK { public int $x { get { throw new RuntimeException("nope"); } } }
$phkK = new PhkK();
try { echo $phkK->x; } catch (RuntimeException $e) { echo "caught:", $e->getMessage(), "\n"; }
class PhkL { public int $age { set(int $v) { if ($v < 0) { throw new InvalidArgumentException("neg"); } $this->age = $v; } } }
$phkL = new PhkL();
$phkL->age = 5;
echo $phkL->age, "\n";
try { $phkL->age = -1; } catch (InvalidArgumentException $e) { echo "bad:", $e->getMessage(), "\n"; }
echo $phkL->age, "\n";
--EXPECT--
V3
TN
HI|2
8
14
Error:Property PhkE::$v is read-only
x
42,21
30,0
a-b-c
[init]
[next]
caught:nope
5
bad:neg
5
