--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reference SOURCE fetches a property for WRITING, the way php's BP_VAR_W does
--FILE--
<?php
/* A reference SOURCE is php's `zend_compile_var(source, BP_VAR_W)`: `$r =& $o->p`,
 * `$a[] =& $o->p`, `[&$o->p]`, `foreach ($o->p as &$v)` and a by-reference
 * destructuring source all fetch the property for WRITING, so a name that is not
 * there is CREATED rather than warned about, and an UNINITIALIZED typed one has a
 * refusal of its own — never the auto-initialize-array rule a dimension write
 * takes. A handler-backed property still hands back a COPY (php has no ptr_ptr for
 * one) and an overloaded one still runs __get behind its notice. */
class RswStore { public $a = 1; }
class RswGet { public function __get($n) { echo "  __get($n)\n"; return 42; } }
class RswInt { public int $t; }
class RswArr { public array $t; }
class RswNul { public ?int $t; }
class RswMix { public mixed $t; }
class RswIter { public iterable $t; }
class RswStat { public static int $s; }
class RswPriv { private $p = 1; protected $q = 2; }
final class RswRo { public function __construct(public readonly int $r) {} }

function rsw(string $label, callable $fn): void {
    set_error_handler(function ($no, $msg) { echo "  W$no: $msg\n"; return true; });
    try { $out = $fn(); } catch (\Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    printf("%-34s -> %s\n", $label, str_replace("\n", '', var_export($out, true)));
}

echo "## a missing property is CREATED, in every spelling of the bind\n";
rsw('=& binds the created slot', function () { $o = new stdClass; $r =& $o->m; $r = 'V'; return get_object_vars($o); });
rsw('...and creates it in silence', function () { $o = new stdClass; $r =& $o->m; return [get_object_vars($o), $r]; });
rsw('two binds share one slot', function () { $o = new stdClass; $r =& $o->m; $s =& $o->m; $r = 1; return [get_object_vars($o), $s]; });
rsw('$a[] =& $o->p', function () { $o = new stdClass; $a = []; $a[] =& $o->p; $a[0] = 1; return json_encode([$a, get_object_vars($o)]); });
rsw('$a["k"] =& $o->p', function () { $o = new stdClass; $a = []; $a['k'] =& $o->p; $a['k'] = 1; return json_encode([$a, get_object_vars($o)]); });
rsw('[&$o->p] array literal', function () { $o = new stdClass; $a = [&$o->p]; $a[0] = 1; return json_encode([$a, get_object_vars($o)]); });
rsw('$x->q =& $o->p', function () { $o = new stdClass; $x = new stdClass; $x->q =& $o->p; $x->q = 1; return json_encode([get_object_vars($x), get_object_vars($o)]); });
rsw('[&$t] = $o->p', function () { $o = new stdClass; [&$t] = $o->p; return get_object_vars($o); });
rsw('foreach ($o->p as &$v)', function () { $o = new stdClass; foreach ($o->p as &$v) {} return get_object_vars($o); });
rsw('$this->p inside a method', function () { $o = new class extends stdClass { public function go() { $r =& $this->w; $r = 'W'; return get_object_vars($this); } }; return $o->go(); });
rsw('a released property keeps its ref', function () { $o = new stdClass; $o->p = 7; $r =& $o->p; unset($o->p); return [$r, get_object_vars($o)]; });
rsw('a declared prop that was unset', function () { $o = new RswStore; unset($o->a); $r =& $o->a; $r = 'V'; return get_object_vars($o); });

echo "## a container UNDER the source is an ordinary write base\n";
rsw('=& $o->arr["k"]', function () { $o = new stdClass; $r =& $o->arr['k']; $r = 1; return get_object_vars($o); });
rsw('=& $o->arr["k"]["j"]', function () { $o = new stdClass; $r =& $o->arr['k']['j']; $r = 1; return get_object_vars($o); });
rsw('=& $o->arr[]', function () { $o = new stdClass; $r =& $o->arr[]; $r = 1; return get_object_vars($o); });
rsw('a present array property', function () { $o = new stdClass; $o->l = [1, 2]; foreach ($o->l as &$v) { $v *= 2; } unset($v); return $o->l; });

echo "## an UNINITIALIZED typed property has php's by-reference rule\n";
rsw('int, non-nullable', function () { $o = new RswInt; $r =& $o->t; return 'bound'; });
rsw('array, non-nullable', function () { $o = new RswArr; $r =& $o->t; return 'bound'; });
rsw('iterable, non-nullable', function () { $o = new RswIter; $r =& $o->t; return 'bound'; });
rsw('?int binds NULL', function () { $o = new RswNul; $r =& $o->t; return [$r, get_object_vars($o)]; });
rsw('mixed binds NULL', function () { $o = new RswMix; $r =& $o->t; return [$r, get_object_vars($o)]; });
rsw('?int writes through the bind', function () { $o = new RswNul; $r =& $o->t; $r = 5; return $o->t; });
rsw('?int keeps its type screen', function () { $o = new RswNul; $r =& $o->t; $r = 'nope'; return $o->t; });
rsw('a static, through ::', function () { $r =& RswStat::$s; return 'bound'; });
rsw('foreach by reference', function () { $o = new RswInt; foreach ($o->t as &$v) {} return 'walked'; });
rsw('a by-reference argument', function () { $o = new RswInt; $f = function (&$x) { $x = 1; }; $f($o->t); return $o->t; });
rsw('...by VALUE it is the read Error', function () { $o = new RswInt; $f = function ($x) { return $x; }; return $f($o->t); });
rsw('a DIMENSION write still auto-inits', function () { $o = new RswArr; $o->t['k'] = 1; return $o->t; });

echo "## what the write context does NOT change\n";
rsw('__get answers behind its notice', function () { $o = new RswGet; $r =& $o->m; $r = 'V'; return $o->m; });
rsw('a native property is a COPY', function () { $iv = new DateInterval('PT5S'); $r =& $iv->s; $r = 9; return $iv->s; });
rsw('an ARRAY_AS_PROPS key is created', function () { $ao = new ArrayObject([], ArrayObject::ARRAY_AS_PROPS); $r =& $ao->z; $r = 3; return $ao->getArrayCopy(); });
rsw('a readonly property is refused', function () { $o = new RswRo(1); $r =& $o->r; return 'bound'; });
rsw('a private one, from outside', function () { $o = new RswPriv; $r =& $o->p; return 'bound'; });
rsw('a protected one, from outside', function () { $o = new RswPriv; $r =& $o->q; return 'bound'; });
rsw('a null intermediate is an Error', function () { $o = new stdClass; $o->n = null; $r =& $o->n->deep; return 'bound'; });
rsw('a plain read still warns', function () { $o = new stdClass; $x = $o->m; return [$x, get_object_vars($o)]; });
rsw('...and so does a by-VALUE list', function () { $o = new stdClass; [$t] = $o->m; return get_object_vars($o); });

echo "## the screens that run BEFORE the by-reference rule\n";
class RswVis { private int $p; protected int $q; }
class RswKid extends RswInt {}
class RswDef { public int $t = 3; }
class RswHook { public int $h { get => 9; set (int $v) {} } }
rsw('private, from outside', function () { $o = new RswVis; $f = function (&$x) { $x = 1; }; $f($o->p); return 'bound'; });
rsw('protected, from outside', function () { $o = new RswVis; $f = function (&$x) { $x = 1; }; $f($o->q); return 'bound'; });
rsw('a readonly one', function () { $o = new RswRo(1); $f = function (&$x) { $x = 1; }; $f($o->r); return 'bound'; });
rsw('a hooked one', function () { $o = new RswHook; $f = function (&$x) { $x = 1; }; $f($o->h); return 'bound'; });
rsw('...and by VALUE', function () { $o = new RswHook; $f = function ($x) { return $x; }; return $f($o->h); });
rsw('an INHERITED uninit slot', function () { $o = new RswKid; $f = function (&$x) { $x = 1; }; $f($o->t); return 'bound'; });
rsw('one that was unset()', function () { $o = new RswDef; unset($o->t); $f = function (&$x) { $x = 1; }; $f($o->t); return 'bound'; });
rsw('a typed slot with a value', function () { $o = new RswDef; $f = function (&$x) { $x = 'nope'; }; $f($o->t); return $o->t; });
rsw('sort() on an uninit array', function () { $o = new RswArr; sort($o->t); return 'sorted'; });
rsw('preg_match into one', function () { $o = new RswArr; preg_match('/(a)/', 'a', $o->t); return $o->t; });
rsw('settype() on one', function () { $o = new RswInt; settype($o->t, 'int'); return 'typed'; });
--EXPECT--
## a missing property is CREATED, in every spelling of the bind
=& binds the created slot          -> array (  'm' => 'V',)
...and creates it in silence       -> array (  0 =>   array (    'm' => NULL,  ),  1 => NULL,)
two binds share one slot           -> array (  0 =>   array (    'm' => 1,  ),  1 => 1,)
$a[] =& $o->p                      -> '[[1],{"p":1}]'
$a["k"] =& $o->p                   -> '[{"k":1},{"p":1}]'
[&$o->p] array literal             -> '[[1],{"p":1}]'
$x->q =& $o->p                     -> '[{"q":1},{"p":1}]'
[&$t] = $o->p                      -> array (  'p' =>   array (    0 => NULL,  ),)
  W2: foreach() argument must be of type array|object, null given
foreach ($o->p as &$v)             -> array (  'p' => NULL,)
$this->p inside a method           -> array (  'w' => 'W',)
a released property keeps its ref  -> array (  0 => 7,  1 =>   array (  ),)
a declared prop that was unset     -> array (  'a' => 'V',)
## a container UNDER the source is an ordinary write base
=& $o->arr["k"]                    -> array (  'arr' =>   array (    'k' => 1,  ),)
=& $o->arr["k"]["j"]               -> array (  'arr' =>   array (    'k' =>     array (      'j' => 1,    ),  ),)
=& $o->arr[]                       -> array (  'arr' =>   array (    0 => 1,  ),)
a present array property           -> array (  0 => 2,  1 => 4,)
## an UNINITIALIZED typed property has php's by-reference rule
int, non-nullable                  -> 'Error: Cannot access uninitialized non-nullable property RswInt::$t by reference'
array, non-nullable                -> 'Error: Cannot access uninitialized non-nullable property RswArr::$t by reference'
iterable, non-nullable             -> 'Error: Cannot access uninitialized non-nullable property RswIter::$t by reference'
?int binds NULL                    -> array (  0 => NULL,  1 =>   array (    't' => NULL,  ),)
mixed binds NULL                   -> array (  0 => NULL,  1 =>   array (    't' => NULL,  ),)
?int writes through the bind       -> 5
?int keeps its type screen         -> 'TypeError: Cannot assign string to reference held by property RswNul::$t of type ?int'
a static, through ::               -> 'Error: Cannot access uninitialized non-nullable property RswStat::$s by reference'
foreach by reference               -> 'Error: Cannot access uninitialized non-nullable property RswInt::$t by reference'
a by-reference argument            -> 'Error: Cannot access uninitialized non-nullable property RswInt::$t by reference'
...by VALUE it is the read Error   -> 'Error: Typed property RswInt::$t must not be accessed before initialization'
a DIMENSION write still auto-inits -> array (  'k' => 1,)
## what the write context does NOT change
  __get(m)
  W8: Indirect modification of overloaded property RswGet::$m has no effect
  __get(m)
__get answers behind its notice    -> 42
a native property is a COPY        -> 5
an ARRAY_AS_PROPS key is created   -> array (  'z' => 3,)
a readonly property is refused     -> 'Error: Cannot indirectly modify readonly property RswRo::$r'
a private one, from outside        -> 'Error: Cannot access private property RswPriv::$p'
a protected one, from outside      -> 'Error: Cannot access protected property RswPriv::$q'
a null intermediate is an Error    -> 'Error: Attempt to modify property "deep" on null'
  W2: Undefined property: stdClass::$m
a plain read still warns           -> array (  0 => NULL,  1 =>   array (  ),)
  W2: Undefined property: stdClass::$m
...and so does a by-VALUE list     -> array ()
## the screens that run BEFORE the by-reference rule
private, from outside              -> 'Error: Cannot access private property RswVis::$p'
protected, from outside            -> 'Error: Cannot access protected property RswVis::$q'
a readonly one                     -> 'Error: Cannot indirectly modify readonly property RswRo::$r'
a hooked one                       -> 'Error: Indirect modification of RswHook::$h is not allowed'
...and by VALUE                    -> 9
an INHERITED uninit slot           -> 'Error: Cannot access uninitialized non-nullable property RswInt::$t by reference'
one that was unset()               -> 'Error: Cannot access uninitialized non-nullable property RswDef::$t by reference'
a typed slot with a value          -> 'TypeError: Cannot assign string to reference held by property RswDef::$t of type int'
sort() on an uninit array          -> 'Error: Cannot access uninitialized non-nullable property RswArr::$t by reference'
preg_match into one                -> 'Error: Cannot access uninitialized non-nullable property RswArr::$t by reference'
settype() on one                   -> 'Error: Cannot access uninitialized non-nullable property RswInt::$t by reference'
