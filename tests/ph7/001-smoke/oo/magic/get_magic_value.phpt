--TEST--
__get returns its value for missing and inaccessible properties
--DESCRIPTION--
Pre-fix, PH7_ClassInstanceCallMagicMethod discarded the __get return value
(every read yielded NULL plus a PHL-native warn even when __get existed).
Covers: basic value, expression use, inaccessible private from outside,
real private read inside the class, throwing __get (boundary routing),
recursion guard (php property guard), nested different-name reads,
isset without __isset, declared-property precedence, subscripting the
result, and feeding a typed parameter.
--FILE--
<?php
class GmvA {
    public function __get($n) { return "v-$n"; }
}
echo (new GmvA())->p, "\n";

class GmvB {
    public function __get($n) { return 5; }
}
$b = new GmvB();
echo $b->p + 1, "\n";

class GmvPriv {
    private $s = 7;
    public function __get($n) { return 99; }
}
echo (new GmvPriv())->s, "\n";   // inaccessible outside -> __get

class GmvSelf {
    private $s = 7;
    public function __get($n) { return $this->s; }  // real private inside
}
echo (new GmvSelf())->x, "\n";

class GmvThrow {
    public function __get($n) { throw new Exception("g:$n"); }
}
try { $x = (new GmvThrow())->p; echo "resumed\n"; }
catch (Exception $e) { echo "caught:", $e->getMessage(), "\n"; }

// php property guard: a self-recursive same-name read falls back to the
// undefined-property path (engine warn text differs -> suppress, keep value)
class GmvRec {
    public function __get($n) { return $this->$n; }
}
set_error_handler(function () { return true; });
$v = (new GmvRec())->x;
restore_error_handler();
var_export($v); echo "\n";

// nested different-name reads still dispatch
class GmvNest {
    public function __get($n) { return $n === "a" ? $this->b : "end"; }
}
echo (new GmvNest())->a, "\n";

// isset consults __isset (absent) -> false, __get NOT called
class GmvIsset {
    public function __get($n) { return 5; }
}
var_export(isset((new GmvIsset())->p)); echo "\n";

// a declared public property wins over __get
class GmvDecl {
    public $r = 1;
    public function __get($n) { return 9; }
}
echo (new GmvDecl())->r, "\n";

// subscript the magic result; feed a typed parameter
class GmvArr {
    public function __get($n) { return [1, 2, 3]; }
}
echo (new GmvArr())->arr[1], "\n";
class GmvStr {
    public function __get($n) { return "s"; }
}
function gmv_take(string $x) { return strtoupper($x); }
echo gmv_take((new GmvStr())->p), "\n";
echo "done\n";
?>
--EXPECT--
v-p
6
99
7
caught:g:p
NULL
end
false
1
2
S
done
