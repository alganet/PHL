--TEST--
Property hooks (PHP 8.4) slice 4: abstract/interface hook declarations, plain-property satisfaction, parent::$x::get()/set()
--FILE--
<?php
// abstract hook declarations, implemented by a concrete subclass
abstract class HaA {
    abstract public int $x { get; set; }
}
class HaB extends HaA {
    public int $x { get => 5; set { } }
}
echo (new HaB())->x, "\n";
// typed explicit set param implementation is compatible with the stub
abstract class HaC {
    abstract public int $y { set; }
}
class HaD extends HaC {
    public int $y = 0 { set(int $v) { $this->y = $v * 2; } }
}
$haD = new HaD();
$haD->y = 4;
echo $haD->y, "\n";
// interface hook requirements
interface HaI {
    public int $v { get; }
}
class HaE implements HaI {
    public int $v { get => 9; }
}
echo (new HaE())->v, "\n";
// a plain property satisfies { get; set; } requirements
interface HaJ {
    public int $w { get; set; }
}
class HaF implements HaJ {
    public int $w = 3;
}
echo (new HaF())->w, "\n";
// ... in abstract classes too
abstract class HaG {
    abstract public int $z { get; }
}
class HaH extends HaG {
    public int $z = 7;
}
echo (new HaH())->z, "\n";
// parent::$x::get() / parent::$x::set() call the parent hook implementations
class HaP {
    public int $p = 2 {
        get => $this->p * 10;
        set { $this->p = $value; }
    }
}
class HaQ extends HaP {
    public int $p = 2 {
        get => parent::$p::get() + 1;
        set { parent::$p::set($value + 100); }
    }
}
$haQ = new HaQ();
echo $haQ->p, "\n";
$haQ->p = 4;
echo $haQ->p, "\n";
?>
--EXPECT--
5
8
9
3
7
21
1041
