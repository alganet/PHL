--TEST--
__set/__unset/__isset dispatch, dynamic-property deprecation, __call/__callStatic
--DESCRIPTION--
Band A #3b: a plain store to a missing or inaccessible property dispatches
__set($name,$value) (no property is created); unset() dispatches __unset for
missing/inaccessible (an inaccessible unset without __unset is a catchable
Error); isset/empty consult __isset (then __get for empty's value); without
__set a dynamic property is created on any class with the 8.2 deprecation
(suppressed for stdClass and #[AllowDynamicProperties]); a missing static
property is a catchable Error (instance magic never consulted); missing and
inaccessible methods route through __call/__callStatic with the collected
argument array.
--FILE--
<?php
// __set: missing + inaccessible; no property created; expression value kept
class SmA {
    private $p = 3;
    public function __set($n, $v) { echo "set:$n=$v\n"; }
}
$a = new SmA();
$a->w = 5;
$a->p = 7;
var_export(property_exists($a, "w")); echo "\n";
$r = ($a->q = 9);
echo "r=", $r, "\n";

// __set throws -> catchable, assignment abandoned
class SmThrow { public function __set($n, $v) { throw new Exception("s"); } }
$st = new SmThrow();
try { $st->w = 5; echo "resumed\n"; } catch (Exception $e) { echo "caught-set\n"; }

// guarded same-name write inside __set falls through to dynamic creation
class SmGuard {
    public function __set($n, $v) { $this->$n = strtoupper($v); }
    public function __get($n) { return "?"; }
}
set_error_handler(function () { return true; }); // swallow the deprecation (engine display formats differ)
$g = new SmGuard();
$g->w = "x";
restore_error_handler();
echo $g->w ?? "none", "\n";

// __unset: missing + inaccessible; Error without it
class SmU {
    private $p = 1;
    public function __unset($n) { echo "unset:$n\n"; }
}
$u = new SmU();
unset($u->missing);
unset($u->p);
class SmU2 { private $p = 1; }
try { $u2 = new SmU2(); unset($u2->p); echo "resumed\n"; }
catch (Error $e) { echo "caught:", $e->getMessage(), "\n"; }

// __isset for isset and empty; inaccessible isset is silently false
class SmI {
    private $hidden = 1;
    public function __isset($n) { echo "isset:$n\n"; return $n === "a"; }
    public function __get($n) { return 0; }
}
$i = new SmI();
var_export(isset($i->a)); echo "\n";
var_export(isset($i->z)); echo "\n";
var_export(empty($i->a)); echo "\n";  // __isset true -> __get 0 -> empty
class SmPriv { private $h = 1; }
var_export(isset((new SmPriv())->h)); echo "\n";

// dynamic property: created with the 8.2 deprecation (observed via handler);
// suppressed for #[AllowDynamicProperties] and stdClass
class SmDyn {}
set_error_handler(function ($no, $s) { echo "dep($no): $s\n"; return true; });
$d = new SmDyn();
$d->w = 5;
#[AllowDynamicProperties] class SmAllow {}
$al = new SmAllow();
$al->w = 6;
$sc = new stdClass();
$sc->w = 7;
restore_error_handler();
echo $d->w, $al->w, $sc->w, "\n";

// static miss: catchable Error, no instance magic consulted
class SmS { public static $s = 1; public function __get($n) { return 9; } }
try { echo SmS::$missing; echo "resumed\n"; }
catch (Error $e) { echo "caught:", $e->getMessage(), "\n"; }
echo SmS::$s, "\n";

// __call / __callStatic with the packed argument array
class SmC {
    private $base = 10;
    private function hidden() { return "h"; }
    public function __call($n, $a) { return "$n(" . implode(",", $a) . ")+" . $this->base; }
    public static function __callStatic($n, $a) { return "S:$n:" . count($a); }
}
$c = new SmC();
echo $c->m(1, 2), "\n";
echo $c->hidden(), "\n";      // inaccessible -> __call (php)
echo SmC::stat(4, 5, 6), "\n";
$args = [7, 8];
echo $c->sp(...$args), "\n";
class SmCT { public function __call($n, $a) { throw new Exception("c:$n"); } }
try { (new SmCT())->boom(); echo "resumed\n"; } catch (Exception $e) { echo "caught:", $e->getMessage(), "\n"; }
echo "done\n";
?>
--EXPECT--
set:w=5
set:p=7
false
set:q=9
r=9
caught-set
X
unset:missing
unset:p
caught:Cannot access private property SmU2::$p
isset:a
true
isset:z
false
isset:a
true
false
dep(8192): Creation of dynamic property SmDyn::$w is deprecated
567
caught:Access to undeclared static property SmS::$missing
1
m(1,2)+10
hidden()+10
S:stat:3
sp(7,8)+10
caught:c:boom
done
