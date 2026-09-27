--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin's by-reference out-parameter reaches a PROPERTY the way php's W fetch does
--FILE--
<?php
/* `preg_match($re, $subject, $this->matches)` is the ordinary spelling, and what
 * php does to that third argument is its `FETCH_OBJ_W` — which is NOT what an
 * assignment does to the same property: a missing one is CREATED, an overloaded
 * one takes a notice and is passed by VALUE (rather than reaching `__set`), a
 * readonly or hooked one is a catchable Error, and a non-object base is the
 * `Attempt to modify property` Error rather than a read warning. */
class BapBase {
    public $decl;
    public $filled = 'old';
    protected $prot;
    private $priv;
    public static $stat = 's';
}
class BapKid extends BapBase {
    public function inside(): array {
        $out = [preg_match('/(a)/', 'a', $this->decl), $this->decl];
        $out[] = preg_match('/(b)/', 'b', $this->prot);
        $out[] = $this->prot;
        return $out;
    }
}
class BapGet { public function __get($n) { return "g:$n"; } }
class BapHook { public array $h { get => ['hooked']; set (array $v) {} } }
class BapRo { public function __construct(public readonly array $r) {} }

function bap(string $label, callable $fn): void {
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = "W$no: $msg"; return true; });
    try { $out = $fn(); } catch (\Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    foreach ($seen as $line) { echo '  ', $line, "\n"; }
    printf("%-30s -> %s\n", $label, str_replace("\n", '', var_export($out, true)));
}

echo "## the property is created, at every depth\n";
bap('missing on stdClass', function () { $o = new stdClass; $r = preg_match('/(a)/', 'a', $o->m); return [$r, $o->m ?? '<none>']; });
bap('missing two levels down', function () { $o = new stdClass; $o->a = new stdClass; $r = preg_match('/(a)/', 'a', $o->a->b); return [$r, $o->a->b ?? '<none>']; });
bap('variable property name', function () { $o = new stdClass; $n = 'dyn'; $r = preg_match('/(a)/', 'a', $o->$n); return [$r, $o->dyn ?? '<none>']; });
bap('declared, never written', function () { $o = new BapBase; $r = preg_match('/(a)/', 'a', $o->decl); return [$r, $o->decl]; });
bap('declared, with a value', function () { $o = new BapBase; $r = preg_match('/(a)/', 'a', $o->filled); return [$r, $o->filled]; });
bap('$this-> inside a method', fn() => (new BapKid)->inside());
bap('a static, through ::', function () { $r = preg_match('/(a)/', 'a', BapBase::$stat); $v = BapBase::$stat; BapBase::$stat = 's'; return [$r, $v]; });
bap('a property then a subscript', function () { $o = new stdClass; $r = preg_match('/(a)/', 'a', $o->a['k']); return [$r, $o->a ?? '<none>']; });
bap('a property then an append', function () { $o = new stdClass; $o->a = []; $r = preg_match('/(a)/', 'a', $o->a[]); return [$r, $o->a]; });

echo "## every other by-ref builtin reaches it the same way\n";
bap('str_replace $count', function () { $o = new stdClass; $s = str_replace('a', 'b', 'aaa', $o->n); return [$s, $o->n ?? '<none>']; });
bap('preg_match_all $matches', function () { $o = new stdClass; $r = preg_match_all('/(a)/', 'aa', $o->m); return [$r, $o->m ?? '<none>']; });
bap('preg_replace $count', function () { $o = new stdClass; $s = preg_replace('/a/', 'b', 'aa', -1, $o->c); return [$s, $o->c ?? '<none>']; });
bap('similar_text $percent', function () { $o = new stdClass; $r = similar_text('abc', 'abd', $o->p); return [$r, $o->p ?? '<none>']; });
bap('parse_str $result', function () { $o = new stdClass; parse_str('a=1&b=2', $o->q); return $o->q ?? '<none>'; });
bap('sscanf variables', function () { $o = new stdClass; $r = sscanf('4 5', '%d %d', $o->a, $o->b); return [$r, $o->a ?? '<none>', $o->b ?? '<none>']; });
bap('is_callable $callable_name', function () { $o = new stdClass; $r = is_callable('strlen', false, $o->nm); return [$r, $o->nm ?? '<none>']; });
bap('settype on a missing one', function () { $o = new stdClass; $r = settype($o->v, 'integer'); return [$r, $o->v ?? '<none>']; });
bap('sort on a property', function () { $o = new stdClass; $o->a = [3, 1]; $r = sort($o->a); return [$r, $o->a]; });

echo "## what cannot be aliased\n";
bap('an overloaded property', function () { $o = new BapGet; $r = preg_match('/(a)/', 'a', $o->m); return [$r]; });
bap('a hooked property', function () { $o = new BapHook; $r = preg_match('/(a)/', 'a', $o->h); return [$r, $o->h]; });
bap('a readonly property', function () { $o = new BapRo(['x']); $r = preg_match('/(a)/', 'a', $o->r); return [$r, $o->r]; });
bap('protected, from outside', function () { $o = new BapBase; return preg_match('/(a)/', 'a', $o->prot); });
bap('private, from outside', function () { $o = new BapBase; return preg_match('/(a)/', 'a', $o->priv); });
bap('a null base', function () { $u = null; $r = preg_match('/(a)/', 'a', $u->p); return [$r, $u]; });
bap('an int base', function () { $u = 7; $r = preg_match('/(a)/', 'a', $u->p); return [$r, $u]; });
bap('a string base', function () { $u = 'str'; $r = preg_match('/(a)/', 'a', $u->p); return [$r, $u]; });
bap('an undefined base variable', function () { $r = preg_match('/(a)/', 'a', $bapNope->p); return [$r, $bapNope ?? '<none>']; });
--EXPECT--
## the property is created, at every depth
missing on stdClass            -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),)
missing two levels down        -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),)
variable property name         -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),)
declared, never written        -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),)
declared, with a value         -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),)
$this-> inside a method        -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),  2 => 1,  3 =>   array (    0 => 'b',    1 => 'b',  ),)
a static, through ::           -> array (  0 => 1,  1 =>   array (    0 => 'a',    1 => 'a',  ),)
a property then a subscript    -> array (  0 => 1,  1 =>   array (    'k' =>     array (      0 => 'a',      1 => 'a',    ),  ),)
a property then an append      -> array (  0 => 1,  1 =>   array (    0 =>     array (      0 => 'a',      1 => 'a',    ),  ),)
## every other by-ref builtin reaches it the same way
str_replace $count             -> array (  0 => 'bbb',  1 => 3,)
preg_match_all $matches        -> array (  0 => 2,  1 =>   array (    0 =>     array (      0 => 'a',      1 => 'a',    ),    1 =>     array (      0 => 'a',      1 => 'a',    ),  ),)
preg_replace $count            -> array (  0 => 'bb',  1 => 2,)
similar_text $percent          -> array (  0 => 2,  1 => 66.66666666666667,)
parse_str $result              -> array (  'a' => '1',  'b' => '2',)
sscanf variables               -> array (  0 => 2,  1 => 4,  2 => 5,)
is_callable $callable_name     -> array (  0 => true,  1 => 'strlen',)
settype on a missing one       -> array (  0 => true,  1 => 0,)
sort on a property             -> array (  0 => true,  1 =>   array (    0 => 1,    1 => 3,  ),)
## what cannot be aliased
  W8: Indirect modification of overloaded property BapGet::$m has no effect
an overloaded property         -> array (  0 => 1,)
a hooked property              -> 'Error: Indirect modification of BapHook::$h is not allowed'
a readonly property            -> 'Error: Cannot indirectly modify readonly property BapRo::$r'
protected, from outside        -> 'Error: Cannot access protected property BapBase::$prot'
private, from outside          -> 'Error: Cannot access private property BapBase::$priv'
a null base                    -> 'Error: Attempt to modify property "p" on null'
an int base                    -> 'Error: Attempt to modify property "p" on int'
a string base                  -> 'Error: Attempt to modify property "p" on string'
an undefined base variable     -> 'Error: Attempt to modify property "p" on null'
