--TEST--
Read-modify-write of an overloaded property reads __get and writes __set
--FILE--
<?php
// php's `$o->n++` on a name the instance does not expose is __get, modify, __set —
// not a property vivification. Both accessors must be declared: with only one of
// them php falls back to creating the property, which is a different path.
class MagicRmwBox {
    private $d = ['n' => 5, 's' => 'ab'];
    public function __get($k) { echo "[get $k]"; return $this->d[$k] ?? null; }
    public function __set($k, $v) { echo "[set $k=", var_export($v, true), "]"; $this->d[$k] = $v; }
    public function __isset($k) { return isset($this->d[$k]); }
}

$m = new MagicRmwBox();
$m->n++;
echo " ", $m->n, "\n";

$m = new MagicRmwBox();
++$m->n;
echo " ", $m->n, "\n";

$m = new MagicRmwBox();
$m->n--;
echo " ", $m->n, "\n";

$m = new MagicRmwBox();
$m->n += 10;
echo " ", $m->n, "\n";

$m = new MagicRmwBox();
$m->n *= 3;
echo " ", $m->n, "\n";

$m = new MagicRmwBox();
$m->s .= "c";
echo " ", $m->s, "\n";

// The expression VALUE of each form, which is the pre-image for post-increment
// and the computed one for the rest.
$m = new MagicRmwBox();
echo "post=", $m->n++, "\n";
$m = new MagicRmwBox();
echo "pre=", ++$m->n, "\n";
$m = new MagicRmwBox();
echo "compound=", ($m->n += 4), "\n";

// A DECLARED but inaccessible name is overloaded from outside the class exactly
// like a missing one: php's accessors answer for it.
class MagicRmwPrivate {
    private $n = 5;
    public function __get($k) { echo "[get $k]"; return 100; }
    public function __set($k, $v) { echo "[set $k=", var_export($v, true), "]"; }
}
$p = new MagicRmwPrivate();
$p->n++;
echo " ", $p->n, "\n";

// Nesting: the RHS of one overloaded compound-assign runs another. Each arm
// consumes its own pending write-back.
class MagicRmwNest {
    public $log = [];
    private $d = ['a' => 1, 'b' => 2];
    public function __get($k) { return $this->d[$k]; }
    public function __set($k, $v) { $this->log[] = "$k=$v"; $this->d[$k] = $v; }
}
$n = new MagicRmwNest();
$n->a += ($n->b += 3);
echo implode(",", $n->log), " a=", $n->a, " b=", $n->b, "\n";

// A throw from __get abandons the statement: no __set runs, and the catch sees it.
class MagicRmwThrow {
    public $sets = 0;
    public function __get($k) { throw new RuntimeException("no $k"); }
    public function __set($k, $v) { $this->sets++; }
}
$t = new MagicRmwThrow();
try {
    $t->z++;
} catch (RuntimeException $e) {
    echo "caught ", $e->getMessage(), " sets=", $t->sets, "\n";
}

// An array/object value cannot be incremented, whichever layer produced it.
class MagicRmwArr {
    public function __get($k) { return [1, 2]; }
    public function __set($k, $v) { echo "UNREACHABLE"; }
}
$a = new MagicRmwArr();
try {
    $a->list++;
} catch (TypeError $e) {
    echo "type: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
[get n][set n=6] [get n]6
[get n][set n=6] [get n]6
[get n][set n=4] [get n]4
[get n][set n=15] [get n]15
[get n][set n=15] [get n]15
[get s][set s='abc'] [get s]abc
post=[get n][set n=6]5
pre=[get n][set n=6]6
compound=[get n][set n=9]9
[get n][set n=101] [get n]100
b=5,a=6 a=6 b=5
caught no z sets=0
type: Cannot increment array
