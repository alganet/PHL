--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compound assign on an ArrayAccess element reads offsetGet and writes offsetSet
--FILE--
<?php
// php compiles `$o[$k] op= v` to ASSIGN_DIM_OP: read the element, compute, write it
// back through the container's own handlers. For an ArrayAccess object that is
// offsetGet($k) then offsetSet($k, $computed) — for every operator in the family.
class DimOpBox implements ArrayAccess {
    public $d;
    public function __construct($d) { $this->d = $d; }
    public function offsetExists($o): bool { return isset($this->d[$o]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($o) { echo "[get ", var_export($o, true), "]"; return $this->d[$o] ?? null; }
    public function offsetSet($o, $v): void {
        echo "[set ", var_export($o, true), "=", var_export($v, true), "]";
        if ($o === null) { $this->d[] = $v; } else { $this->d[$o] = $v; }
    }
    public function offsetUnset($o): void { unset($this->d[$o]); }
}
function dimOpFresh() { return new DimOpBox(['n' => 5, 's' => 'ab']); }

foreach (['+=', '-=', '*=', '/=', '%=', '**=', '<<=', '>>=', '&=', '|=', '^='] as $op) {
    $o = dimOpFresh();
    echo str_pad($op, 4);
    eval('$o["n"] ' . $op . ' 2;');
    echo " -> ", var_export($o->d['n'], true), "\n";
}
$o = dimOpFresh();
echo ".=  ";
$o['s'] .= "c";
echo " -> ", var_export($o->d['s'], true), "\n";

// The expression VALUE is the computed one.
$o = dimOpFresh();
echo "expr=", var_export($o['n'] += 3, true), "\n";

// A MISSING key is the class's own business: php reads it (whatever the class
// answers for it) and writes the result back.
$o = dimOpFresh();
$o['fresh'] += 3;
echo " -> ", var_export($o->d['fresh'], true), "\n";

// `$o[] op= v` hands both accessors the NULL offset php substitutes for the
// absent one — not a missing argument. (The read of that null offset is php's
// own E_DEPRECATED, which both engines raise here and which is not what this
// test is pinning.)
$o = new DimOpBox([]);
$dimOpEr = error_reporting(E_ALL & ~E_DEPRECATED);
$o[] .= "x";
error_reporting($dimOpEr);
echo " -> ", var_export($o->d, true), "\n";

// A throw from the operator abandons the write: offsetSet never runs.
$o = dimOpFresh();
try {
    $o['n'] /= 0;
} catch (DivisionByZeroError $e) {
    echo "caught ", $e->getMessage(), " n=", var_export($o->d['n'], true), "\n";
}

// Nesting: the RHS of one element's compound assign drives another's.
$o = dimOpFresh();
$o['n'] += ($o['n'] *= 2);
echo " -> ", var_export($o->d['n'], true), "\n";

// The natives take the same route, and a subclass's override is seen by it.
$ao = new ArrayObject(['n' => 5]);
$ao['n'] += 2;
echo "ArrayObject -> ", var_export($ao['n'], true), "\n";
class DimOpArrayObject extends ArrayObject {
    public function offsetSet($k, $v): void { echo "[sub]"; parent::offsetSet($k, $v); }
}
$sub = new DimOpArrayObject(['n' => 5]);
$sub['n'] += 2;
echo " -> ", var_export($sub['n'], true), "\n";
?>
--EXPECT--
+=  [get 'n'][set 'n'=7] -> 7
-=  [get 'n'][set 'n'=3] -> 3
*=  [get 'n'][set 'n'=10] -> 10
/=  [get 'n'][set 'n'=2.5] -> 2.5
%=  [get 'n'][set 'n'=1] -> 1
**= [get 'n'][set 'n'=25] -> 25
<<= [get 'n'][set 'n'=20] -> 20
>>= [get 'n'][set 'n'=1] -> 1
&=  [get 'n'][set 'n'=0] -> 0
|=  [get 'n'][set 'n'=7] -> 7
^=  [get 'n'][set 'n'=7] -> 7
.=  [get 's'][set 's'='abc'] -> 'abc'
expr=[get 'n'][set 'n'=8]8
[get 'fresh'][set 'fresh'=3] -> 3
[get NULL][set NULL='x'] -> array (
  0 => 'x',
)
[get 'n']caught Division by zero n=5
[get 'n'][set 'n'=10][get 'n'][set 'n'=20] -> 20
ArrayObject -> 7
[sub] -> 7
