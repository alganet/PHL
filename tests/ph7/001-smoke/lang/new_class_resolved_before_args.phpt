--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`new` resolves and refuses its class BEFORE the constructor arguments
--DESCRIPTION--
php's NEW resolves the class, refuses everything a `new` can be refused for and
allocates the object; only the constructor BODY runs after the arguments. PHL evaluated
the whole argument list first and did all of it inside one OP_NEW, so `new NoSuchClass(s(1))`,
`new AbstractC(s(1))`, `new SomeInterface(s(1))` and `new PrivateCtorC(s(1))` each ran
`s(1)` on a `new` php never performs — and when an argument threw, ITS exception replaced
php's Error. Two neighbours from the same refusal path ride along: a TRAIT was reported as
`Class "T" not found` instead of php's `Cannot instantiate trait T`, and a genuinely missing
class ran every registered autoloader TWICE (the message needed a second lookup, and that
lookup autoloads).
--FILE--
<?php
function ncrArg($x) { echo "  arg$x\n"; return $x; }
function ncrTry($label, $fn) {
    echo $label, ":\n";
    try {
        $r = $fn();
        if ($r !== null) { echo "  => ", var_export($r, true), "\n"; }
    } catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

class NcrOk { public $v = ''; public function __construct(...$a) { $this->v = implode(',', $a); } }
class NcrKid extends NcrOk {}
abstract class NcrAbstract {}
interface NcrInterface {}
trait NcrTrait {}
enum NcrEnum { case A; }
class NcrPrivate {
    private function __construct($a) { $this->v = $a; }
    public $v;
    public static function make($a) { return new self(ncrArg($a)); }
}

/* Refused where php refuses it — the argument never runs. */
ncrTry('missing',     fn() => new NcrNoSuchClass(ncrArg(1)));
ncrTry('abstract',    fn() => new NcrAbstract(ncrArg(2)));
ncrTry('interface',   fn() => new NcrInterface(ncrArg(3)));
ncrTry('trait',       fn() => new NcrTrait(ncrArg(4)));
ncrTry('enum',        fn() => new NcrEnum(ncrArg(5)));
ncrTry('private ctor', fn() => new NcrPrivate(ncrArg(6)));
ncrTry('dynamic missing', function () { $c = 'NcrStillMissing'; return new $c(ncrArg(7)); });
ncrTry('throwing arg',    fn() => new NcrNoSuchClass(ncrArg(8), (function () {
    throw new RuntimeException('from-arg');
})()));

/* Every shape that DOES construct still does, arguments and all. */
ncrTry('literal',      fn() => (new NcrOk(ncrArg('a'), ncrArg('b')))->v);
ncrTry('subclass',     fn() => (new NcrKid(ncrArg('c')))->v);
ncrTry('no parens',    function () { $o = new NcrOk; return $o->v; });
ncrTry('spread',       function () { $a = ['d', 'e']; return (new NcrOk(...$a))->v; });
ncrTry('spread empty', function () { $a = []; return (new NcrOk(...$a))->v; });
ncrTry('dynamic name', function () { $c = 'NcrOk'; return (new $c(ncrArg('f')))->v; });
ncrTry('from object',  function () { $o = new NcrOk; return (new $o(ncrArg('g')))->v; });
ncrTry('new self',     fn() => NcrPrivate::make('h')->v);
ncrTry('anonymous',    function () {
    $o = new class (ncrArg('i')) { public $z; public function __construct($a) { $this->z = $a; } };
    return $o->z;
});
ncrTry('nested',       fn() => (new NcrOk((new NcrOk(ncrArg('j')))->v))->v);

/* The autoloader runs once per missing name, as php runs it. Registered and
 * unregistered around the one probe: this corpus shares a single interpreter, so a
 * loader left standing would answer for every later test's class lookups. */
$ncrLoader = function ($c) { echo "  autoload($c)\n"; };
spl_autoload_register($ncrLoader);
ncrTry('autoload once', fn() => new NcrAutoloadMe(ncrArg('k')));
spl_autoload_unregister($ncrLoader);
echo "end\n";
?>
--EXPECT--
missing:
  Error: Class "NcrNoSuchClass" not found
abstract:
  Error: Cannot instantiate abstract class NcrAbstract
interface:
  Error: Cannot instantiate interface NcrInterface
trait:
  Error: Cannot instantiate trait NcrTrait
enum:
  Error: Cannot instantiate enum NcrEnum
private ctor:
  Error: Call to private NcrPrivate::__construct() from global scope
dynamic missing:
  Error: Class "NcrStillMissing" not found
throwing arg:
  Error: Class "NcrNoSuchClass" not found
literal:
  arga
  argb
  => 'a,b'
subclass:
  argc
  => 'c'
no parens:
  => ''
spread:
  => 'd,e'
spread empty:
  => ''
dynamic name:
  argf
  => 'f'
from object:
  argg
  => 'g'
new self:
  argh
  => 'h'
anonymous:
  argi
  => 'i'
nested:
  argj
  => 'j'
autoload once:
  autoload(NcrAutoloadMe)
  Error: Class "NcrAutoloadMe" not found
end
