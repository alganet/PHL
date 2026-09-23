--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A call refuses an unusable callee BEFORE it evaluates the arguments
--DESCRIPTION--
php resolves a call's target at its INIT_FCALL / INIT_DYNAMIC_CALL, so an undefined
function, a callable string or array naming nothing, and a value that is not callable
at all are all refused before a single argument runs. PHL only ever looked at the callee
inside OP_CALL, one instruction after the whole argument list, so every one of these
programs produced the argument's side effects first and php's Error second. The messages
were already identical; only the order was not. The verdict is the one the first-class
callable creation screen already gives, which is php's rule too: `f(...)` refuses exactly
what `f()` refuses, word for word.
--FILE--
<?php
function ccrArg($x) { echo "  arg$x\n"; return $x; }
function ccrTry($label, $fn) {
    echo $label, ":\n";
    try {
        $r = $fn();
        if ($r !== null) { echo "  => ", var_export($r, true), "\n"; }
    } catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

class CcrHost {
    private function priv($a) { return "priv($a)"; }
    public function pub($a) { return "pub($a)"; }
    public static function stat($a) { return "stat($a)"; }
    public function __invoke($a) { return "inv($a)"; }
    public function insideArray() { $f = [$this, 'priv']; return $f(ccrArg('P')); }
    public function insideString() { $f = 'CcrHost::priv'; return $f(ccrArg('Q')); }
}
class CcrMagic {
    public function __call($n, $a) { return "call:$n(" . implode(',', $a) . ')'; }
    public static function __callStatic($n, $a) { return "static:$n(" . implode(',', $a) . ')'; }
}

/* Refused where php refuses it — the argument never runs. */
ccrTry('undefined function', fn() => ccrNoSuchFunction(ccrArg(1)));
ccrTry('string name miss',   function () { $f = 'ccrNope'; return $f(ccrArg(2)); });
ccrTry('array bad method',   function () { $f = [new CcrHost, 'nope']; return $f(ccrArg(3)); });
ccrTry('array bad shape',    function () { $f = [1]; return $f(ccrArg(4)); });
ccrTry('array unknown cls',  function () { $f = ['CcrNoSuchClass', 'm']; return $f(ccrArg(5)); });
ccrTry('string cls::miss',   function () { $f = 'CcrHost::nope'; return $f(ccrArg(6)); });
ccrTry('string cls::nonstatic', function () { $f = 'CcrHost::pub'; return $f(ccrArg(7)); });
ccrTry('int callee',         function () { $f = 5; return $f(ccrArg(8)); });
ccrTry('null callee',        function () { $f = null; return $f(ccrArg(9)); });
ccrTry('array value callee', function () { $f = [1, 2, 3]; return $f(ccrArg(10)); });
ccrTry('private outside',    function () { $f = [new CcrHost, 'priv']; return $f(ccrArg(11)); });
ccrTry('nonstatic inside',   fn() => (new CcrHost)->insideString());

/* Everything callable still runs, arguments and all. */
ccrTry('plain name',    fn() => strtoupper(ccrArg('a')));
ccrTry('string var',    function () { $f = 'strtolower'; return $f(ccrArg('B')); });
ccrTry('array object',  function () { $f = [new CcrHost, 'pub']; return $f(ccrArg('c')); });
ccrTry('array class',   function () { $f = ['CcrHost', 'stat']; return $f(ccrArg('d')); });
ccrTry('string static', function () { $f = 'CcrHost::stat'; return $f(ccrArg('e')); });
ccrTry('__invoke',      function () { $f = new CcrHost; return $f(ccrArg('f')); });
ccrTry('closure',       function () { $f = fn($x) => "cl($x)"; return $f(ccrArg('g')); });
ccrTry('fcc',           function () { $f = strrev(...); return $f(ccrArg('hi')); });
ccrTry('private inside', fn() => (new CcrHost)->insideArray());

/* A class with a catch-all is callable for ANY name, missing or inaccessible, so the
 * screen must stand down for it — the routing is what php does with the call. */
ccrTry('magic object', function () { $f = [new CcrMagic, 'zz']; return $f(ccrArg('m')); });
ccrTry('magic class',  function () { $f = ['CcrMagic', 'zz']; return $f(ccrArg('n')); });
ccrTry('magic string', function () { $f = 'CcrMagic::zz'; return $f(ccrArg('o')); });
echo "end\n";
?>
--EXPECT--
undefined function:
  Error: Call to undefined function ccrNoSuchFunction()
string name miss:
  Error: Call to undefined function ccrNope()
array bad method:
  Error: Call to undefined method CcrHost::nope()
array bad shape:
  Error: Array callback must have exactly two elements
array unknown cls:
  Error: Class "CcrNoSuchClass" not found
string cls::miss:
  Error: Call to undefined method CcrHost::nope()
string cls::nonstatic:
  Error: Non-static method CcrHost::pub() cannot be called statically
int callee:
  Error: Value of type int is not callable
null callee:
  Error: Value of type null is not callable
array value callee:
  Error: Array callback must have exactly two elements
private outside:
  Error: Call to private method CcrHost::priv() from global scope
nonstatic inside:
  Error: Non-static method CcrHost::priv() cannot be called statically
plain name:
  arga
  => 'A'
string var:
  argB
  => 'b'
array object:
  argc
  => 'pub(c)'
array class:
  argd
  => 'stat(d)'
string static:
  arge
  => 'stat(e)'
__invoke:
  argf
  => 'inv(f)'
closure:
  argg
  => 'cl(g)'
fcc:
  arghi
  => 'ih'
private inside:
  argP
  => 'priv(P)'
magic object:
  argm
  => 'call:zz(m)'
magic class:
  argn
  => 'static:zz(n)'
magic string:
  argo
  => 'static:zz(o)'
end
