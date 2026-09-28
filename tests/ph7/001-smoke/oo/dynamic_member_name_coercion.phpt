--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A dynamic member NAME is coerced (property) or refused (method/class) php's way
--FILE--
<?php
/* php decides what a `{expr}` NAME may BE before it looks at the receiver, and
 * the three name positions decide differently:
 *   - a PROPERTY name is coerced with the user-visible rules (an array warns
 *     "Array to string conversion" and renders "Array"; an object hands over
 *     its __toString(), and one without it is a catchable Error);
 *   - a METHOD name must ALREADY be a string ("Method name must be a string"),
 *     refused before the receiver's type, before __call, before any lookup;
 *   - a CLASS name must be an object or a string
 *     ("Class name must be a valid object or a string") at `new`, `::` and
 *     `instanceof` alike.
 * php's one exception is the shape that answers before it asks for the name:
 * a lookup (isset/empty/`??`) or an unset() on a NON-object short-circuits, so
 * nothing is coerced and nothing is raised. */
#[AllowDynamicProperties]
class DmnBag {
    public $q = 'q-val';
    public $Array = 'array-val';
    public $five = 'five-val';
    public static $sq = 'static-q';
    public static $sArray = 'static-Array';
    const CK = 'const-k';
    public function q() { return 'meth-q'; }
    public static function sm() { return 'static-m'; }
}
class DmnStr   { public function __toString(): string { return 'q'; } }
class DmnVis {
    private $hidden = 'h';
    protected static $prot = 'p';
    private const SECRET = 's';
}
class DmnPlain { }
class DmnMagic {
    public function __get($n)      { return "get<$n>"; }
    public function __call($n, $a) { return "call<$n>"; }
}

function dmn(string $label, callable $fn): void {
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = "E$no: $msg"; return true; });
    try { $out = $fn(); } catch (\Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    printf("%-34s -> %s\n", $label, str_replace("\n", '', var_export($out, true)));
    foreach ($seen as $line) { echo '    ', $line, "\n"; }
}

echo "## a property name is COERCED, and the coercion is user-visible\n";
$arr = [1]; $obj = new DmnStr; $bad = new DmnPlain;
dmn('array names "Array"',   function () use ($arr) { $o = new DmnBag; return $o->{$arr}; });
dmn('int spells itself',     function () { $o = new DmnBag; $n = 5; $o->{$n} = 'i'; return $o->{'5'}; });
dmn('float spells itself',   function () { $o = new DmnBag; $n = 1.5; $o->{$n} = 'f'; return $o->{'1.5'}; });
dmn('true is "1"',           function () { $o = new DmnBag; $o->{true} = 't'; return $o->{'1'}; });
dmn('false is ""',           function () { $o = new DmnBag; $o->{false} = 'f'; return $o->{''}; });
dmn('null is ""',            function () { $o = new DmnBag; $o->{null} = 'n'; return $o->{''}; });
dmn('__toString is taken',   function () use ($obj) { $o = new DmnBag; return $o->{$obj}; });
dmn('no __toString throws',  function () use ($bad) { $o = new DmnBag; return $o->{$bad}; });
dmn('a magic __get sees it', function () use ($arr) { $o = new DmnMagic; return $o->{$arr}; });

echo "## every access shape on an OBJECT coerces\n";
dmn('write',    function () use ($arr) { $o = new DmnBag; $o->{$arr} = 'w'; return $o->Array; });
dmn('isset',    function () use ($arr) { $o = new DmnBag; return isset($o->{$arr}); });
dmn('empty',    function () use ($arr) { $o = new DmnBag; return empty($o->{$arr}); });
dmn('coalesce', function () use ($arr) { $o = new DmnBag; return $o->{$arr} ?? 'd'; });
dmn('unset',    function () use ($arr) { $o = new DmnBag; unset($o->{$arr}); return isset($o->Array); });
dmn('compound', function () use ($arr) { $o = new DmnBag; $o->{$arr} .= '!'; return $o->Array; });
dmn('by-ref',   function () use ($arr) { $o = new DmnBag; $r = &$o->{$arr}; $r = 'r'; return $o->Array; });
dmn('nullsafe', function () use ($arr) { $o = new DmnBag; return $o?->{$arr}; });
dmn('list target', function () use ($arr) { $o = new DmnBag; [$o->{$arr}] = ['l']; return $o->Array; });

echo "## a NON-object receiver: the lookups short-circuit, the rest coerce\n";
dmn('read on int',     function () use ($arr) { $x = 5; return $x->{$arr}; });
dmn('isset on int',    function () use ($arr) { $x = 5; return isset($x->{$arr}); });
dmn('empty on int',    function () use ($arr) { $x = 5; return empty($x->{$arr}); });
dmn('coalesce on int', function () use ($arr) { $x = 5; return $x->{$arr} ?? 'd'; });
dmn('unset on int',    function () use ($arr) { $x = 5; unset($x->{$arr}); return 'ok'; });
dmn('write on int',    function () use ($arr) { $x = 5; $x->{$arr} = 1; return 'ok'; });
dmn('nullsafe on null',function () use ($arr) { $x = null; return $x?->{$arr}; });

echo "## a METHOD name must already BE a string\n";
dmn('array',            function () use ($arr) { $o = new DmnBag; return $o->{$arr}(); });
dmn('__toString object',function () use ($obj) { $o = new DmnBag; return $o->{$obj}(); });
dmn('int',              function () { $o = new DmnBag; $n = 5; return $o->{$n}(); });
dmn('before the receiver check', function () use ($arr) { $x = 5; return $x->{$arr}(); });
dmn('before __call',    function () use ($arr) { $o = new DmnMagic; return $o->{$arr}(); });
dmn('a real string works', function () { $o = new DmnBag; $m = 'q'; return $o->{$m}(); });

echo "## the STATIC side: the class resolves first, then the name\n";
dmn('static property coerces', function () use ($arr) { return DmnBag::${$arr}; });
dmn('static prop __toString',  function () { $s = new class { public function __toString(): string { return 'sq'; } }; return DmnBag::${$s}; });
dmn('static method refused',   function () use ($arr) { return DmnBag::{$arr}(); });
dmn('static method string',    function () { $m = 'sm'; return DmnBag::{$m}(); });
dmn('missing class wins',      function () use ($arr) { return DmnNoSuchClass::${$arr}; });

echo "## `::` reads the class's PROPERTY table: visibility first, then static-ness\n";
/* php's `C::$name` looks the name up among ALL the class's properties and then
 * refuses what is not a static one -- so an instance property reached through
 * `::` is `Access to undeclared static property`, a PRIVATE one is the
 * visibility Error instead, and neither ever hands back a value. A class
 * CONSTANT, unlike a static property, has no silent-lookup fetch at all: it
 * raises in `empty()` and `??` exactly as it does in a plain read. */
dmn('public instance property',   function () { return DmnBag::$q; });
dmn('write to one',               function () { DmnBag::$q = 'w'; return 'ok'; });
dmn('private instance property',  function () { return DmnVis::$hidden; });
dmn('a constant named with $',    function () { return DmnBag::$CK; });
dmn('a real static still reads',  function () { return DmnBag::$sq; });
dmn('protected static, outside',  function () { return DmnVis::$prot; });
dmn('private constant, outside',  function () { return DmnVis::SECRET; });
dmn('isset over a static',        function () { return isset(DmnVis::$prot); });
dmn('empty over a static',        function () { return empty(DmnVis::$prot); });
dmn('coalesce over a static',     function () { return DmnVis::$prot ?? 'd'; });
dmn('empty over a constant',      function () { return empty(DmnVis::SECRET); });
dmn('coalesce over a constant',   function () { return DmnVis::SECRET ?? 'd'; });
dmn('coalesce over a missing one',function () { return DmnVis::NOPE ?? 'd'; });

echo "## a CLASS name must be an object or a string\n";
foreach (['int' => 5, 'float' => 1.5, 'true' => true, 'false' => false, 'null' => null, 'array' => [1]] as $kind => $cls) {
    dmn("new $kind",        function () use ($cls) { return get_class(new $cls); });
    dmn("$kind::method()",  function () use ($cls) { return $cls::sm(); });
    dmn("$kind::\$prop",    function () use ($cls) { return $cls::$sq; });
    dmn("$kind::CONST",     function () use ($cls) { return $cls::CK; });
    dmn("instanceof $kind", function () use ($cls) { return (new DmnBag) instanceof $cls; });
    dmn("scalar instanceof $kind", function () use ($cls) { return 5 instanceof $cls; });
}
dmn('an empty string is still a lookup', function () { $c = ''; return new $c; });
dmn('an object is the class',            function () { $c = new DmnBag; return get_class(new $c); });
dmn('an object on the right of instanceof', function () { return (new DmnBag) instanceof (new DmnBag); });

echo "## a CONSTANT subject folds the whole instanceof away, class operand and all
";
/* php's compiler answers FALSE for `<const> instanceof <anything>` without ever
 * compiling the class operand, so the refusal above never happens for one -- and
 * the operand's own side effects never happen either. (php's folder also reaches
 * a userland constant and a ct-evaluated call; PLAN.md §7.2 records those two.) */
function dmnSide() { echo "    (class operand ran)\n"; return 'DmnBag'; }
dmn('literal subject',   function () use ($arr) { return 5 instanceof $arr; });
dmn('null subject',      function () use ($arr) { return null instanceof $arr; });
dmn('true subject',      function () use ($arr) { return true instanceof $arr; });
dmn('array subject',     function () use ($arr) { return [1] instanceof $arr; });
dmn('arithmetic subject',function () use ($arr) { return (1 + 2) instanceof $arr; });
dmn('concat subject',    function () use ($arr) { return ('a' . 'b') instanceof $arr; });
dmn('the operand is not even run', function () { return 5 instanceof (dmnSide()); });
dmn('a variable subject is not folded', function () use ($arr) { $v = 5; return $v instanceof $arr; });
dmn('a call subject is not folded',     function () use ($arr) { return strrev('a') instanceof $arr; });
--EXPECT--
## a property name is COERCED, and the coercion is user-visible
array names "Array"                -> 'array-val'
    E2: Array to string conversion
int spells itself                  -> 'i'
float spells itself                -> 'f'
true is "1"                        -> 't'
false is ""                        -> 'f'
null is ""                         -> 'n'
__toString is taken                -> 'q-val'
no __toString throws               -> 'Error: Object of class DmnPlain could not be converted to string'
a magic __get sees it              -> 'get<Array>'
    E2: Array to string conversion
## every access shape on an OBJECT coerces
write                              -> 'w'
    E2: Array to string conversion
isset                              -> true
    E2: Array to string conversion
empty                              -> false
    E2: Array to string conversion
coalesce                           -> 'array-val'
    E2: Array to string conversion
unset                              -> false
    E2: Array to string conversion
compound                           -> 'array-val!'
    E2: Array to string conversion
by-ref                             -> 'r'
    E2: Array to string conversion
nullsafe                           -> 'array-val'
    E2: Array to string conversion
list target                        -> 'l'
    E2: Array to string conversion
## a NON-object receiver: the lookups short-circuit, the rest coerce
read on int                        -> NULL
    E2: Array to string conversion
    E2: Attempt to read property "Array" on int
isset on int                       -> false
empty on int                       -> true
coalesce on int                    -> 'd'
unset on int                       -> 'ok'
write on int                       -> 'Error: Attempt to assign property "Array" on int'
    E2: Array to string conversion
nullsafe on null                   -> NULL
## a METHOD name must already BE a string
array                              -> 'Error: Method name must be a string'
__toString object                  -> 'Error: Method name must be a string'
int                                -> 'Error: Method name must be a string'
before the receiver check          -> 'Error: Method name must be a string'
before __call                      -> 'Error: Method name must be a string'
a real string works                -> 'meth-q'
## the STATIC side: the class resolves first, then the name
static property coerces            -> 'Error: Access to undeclared static property DmnBag::$Array'
    E2: Array to string conversion
static prop __toString             -> 'static-q'
static method refused              -> 'Error: Method name must be a string'
static method string               -> 'static-m'
missing class wins                 -> 'Error: Class "DmnNoSuchClass" not found'
## `::` reads the class's PROPERTY table: visibility first, then static-ness
public instance property           -> 'Error: Access to undeclared static property DmnBag::$q'
write to one                       -> 'Error: Access to undeclared static property DmnBag::$q'
private instance property          -> 'Error: Cannot access private property DmnVis::$hidden'
a constant named with $            -> 'Error: Access to undeclared static property DmnBag::$CK'
a real static still reads          -> 'static-q'
protected static, outside          -> 'Error: Cannot access protected property DmnVis::$prot'
private constant, outside          -> 'Error: Cannot access private constant DmnVis::SECRET'
isset over a static                -> false
empty over a static                -> true
coalesce over a static             -> 'd'
empty over a constant              -> 'Error: Cannot access private constant DmnVis::SECRET'
coalesce over a constant           -> 'Error: Cannot access private constant DmnVis::SECRET'
coalesce over a missing one        -> 'Error: Undefined constant DmnVis::NOPE'
## a CLASS name must be an object or a string
new int                            -> 'Error: Class name must be a valid object or a string'
int::method()                      -> 'Error: Class name must be a valid object or a string'
int::$prop                         -> 'Error: Class name must be a valid object or a string'
int::CONST                         -> 'Error: Class name must be a valid object or a string'
instanceof int                     -> 'Error: Class name must be a valid object or a string'
scalar instanceof int              -> false
new float                          -> 'Error: Class name must be a valid object or a string'
float::method()                    -> 'Error: Class name must be a valid object or a string'
float::$prop                       -> 'Error: Class name must be a valid object or a string'
float::CONST                       -> 'Error: Class name must be a valid object or a string'
instanceof float                   -> 'Error: Class name must be a valid object or a string'
scalar instanceof float            -> false
new true                           -> 'Error: Class name must be a valid object or a string'
true::method()                     -> 'Error: Class name must be a valid object or a string'
true::$prop                        -> 'Error: Class name must be a valid object or a string'
true::CONST                        -> 'Error: Class name must be a valid object or a string'
instanceof true                    -> 'Error: Class name must be a valid object or a string'
scalar instanceof true             -> false
new false                          -> 'Error: Class name must be a valid object or a string'
false::method()                    -> 'Error: Class name must be a valid object or a string'
false::$prop                       -> 'Error: Class name must be a valid object or a string'
false::CONST                       -> 'Error: Class name must be a valid object or a string'
instanceof false                   -> 'Error: Class name must be a valid object or a string'
scalar instanceof false            -> false
new null                           -> 'Error: Class name must be a valid object or a string'
null::method()                     -> 'Error: Class name must be a valid object or a string'
null::$prop                        -> 'Error: Class name must be a valid object or a string'
null::CONST                        -> 'Error: Class name must be a valid object or a string'
instanceof null                    -> 'Error: Class name must be a valid object or a string'
scalar instanceof null             -> false
new array                          -> 'Error: Class name must be a valid object or a string'
array::method()                    -> 'Error: Class name must be a valid object or a string'
array::$prop                       -> 'Error: Class name must be a valid object or a string'
array::CONST                       -> 'Error: Class name must be a valid object or a string'
instanceof array                   -> 'Error: Class name must be a valid object or a string'
scalar instanceof array            -> false
an empty string is still a lookup  -> 'Error: Class "" not found'
an object is the class             -> 'DmnBag'
an object on the right of instanceof -> true
## a CONSTANT subject folds the whole instanceof away, class operand and all
literal subject                    -> false
null subject                       -> false
true subject                       -> false
array subject                      -> false
arithmetic subject                 -> false
concat subject                     -> false
the operand is not even run        -> false
a variable subject is not folded   -> 'Error: Class name must be a valid object or a string'
a call subject is not folded       -> 'Error: Class name must be a valid object or a string'
