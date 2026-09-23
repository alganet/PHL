--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A method call resolves its callee BEFORE it evaluates the arguments
--DESCRIPTION--
php resolves `$o->m(...)` at its INIT_METHOD_CALL, so an undefined or inaccessible
method, a member call on a non-object and an unknown class are all refused before a
single argument runs — and a `?->` on null skips the argument list entirely. PHL
emitted the argument list first and the member resolution after it, so every argument's
side effects happened, and when an argument threw, ITS exception was the one the program
saw instead of php's Error. The diagnostics were already byte-identical; only the order
was not.
--FILE--
<?php
function cbrArg($x) { echo "  arg$x\n"; return $x; }
function cbrBoom() { throw new RuntimeException("from-arg"); }
function cbrTry($label, $fn) {
    echo $label, ":\n";
    try { $fn(); } catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

class CbrHost {
    private function priv($a) { return "priv"; }
    public function ok($a) { return "ok($a)"; }
    public static function stat($a) { return "stat($a)"; }
}
class CbrKid extends CbrHost {}

/* The refusal wins over the argument's own side effects — and over its throw. */
cbrTry('undefined-method',   function () { (new CbrHost)->nope(cbrArg(1)); });
cbrTry('private-method',     function () { (new CbrHost)->priv(cbrArg(2)); });
cbrTry('private-throwing',   function () { (new CbrHost)->priv(cbrBoom()); });
cbrTry('undefined-static',   function () { CbrHost::nope(cbrArg(3)); });
cbrTry('unknown-class',      function () { CbrNoSuchClass::m(cbrArg(4)); });
cbrTry('member-on-int',      function () { $v = 5; $v->m(cbrArg(5)); });
cbrTry('member-on-null',     function () { $v = null; $v->m(cbrArg(6)); });
cbrTry('dynamic-name',       function () { $n = 'nope'; (new CbrHost)->$n(cbrArg(7)); });

/* A nullsafe short-circuit never reaches the argument list. */
$null = null;
echo "nullsafe:\n";
var_dump($null?->m(cbrArg(8)));

/* What still runs, runs in php's order: the receiver, then the arguments left to right. */
function cbrRecv() { echo "  receiver\n"; return new CbrHost; }
echo "order:", cbrRecv()->ok(cbrArg(9)), "\n";
echo "static:", CbrKid::stat(cbrArg(10)), "\n";

/* A routed __call nested in another routed call's ARGUMENT list keeps its own
 * {receiver, class, name} — the arguments are evaluated between the two now. */
class CbrMagic {
    public function __call($n, $a) { return "call:$n(" . implode(',', $a) . ')'; }
    public static function __callStatic($n, $a) { return "static:$n(" . implode(',', $a) . ')'; }
}
$m = new CbrMagic;
echo $m->outer($m->inner(11)), "\n";
echo CbrMagic::sOuter(CbrMagic::sInner(12), $m->mixed(13)), "\n";
echo "end\n";
?>
--EXPECT--
undefined-method:
  Error: Call to undefined method CbrHost::nope()
private-method:
  Error: Call to private method CbrHost::priv() from global scope
private-throwing:
  Error: Call to private method CbrHost::priv() from global scope
undefined-static:
  Error: Call to undefined method CbrHost::nope()
unknown-class:
  Error: Class "CbrNoSuchClass" not found
member-on-int:
  Error: Call to a member function m() on int
member-on-null:
  Error: Call to a member function m() on null
dynamic-name:
  Error: Call to undefined method CbrHost::nope()
nullsafe:
NULL
order:  receiver
  arg9
ok(9)
static:  arg10
stat(10)
call:outer(call:inner(11))
static:sOuter(static:sInner(12),call:mixed(13))
end
