--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Rebinding a Closure created from a method follows php's four refusals
--FILE--
<?php
class CrbmBase {
    private function priv() { return "priv@" . get_class($this); }
    public function mk() { return $this->priv(...); }
    private static function sPriv() { return "sPriv@" . static::class; }
    public static function mkStatic() { return self::sPriv(...); }
}
class CrbmKid extends CrbmBase {}
class CrbmOther {}

$o = new CrbmBase();
$kid = new CrbmKid();
$other = new CrbmOther();

function crbm($label, callable $make) {
    $c = $make();
    if ($c === null) {
        echo $label, ":NULL\n";
        return;
    }
    echo $label, ":", $c(), "\n";
}

/* A rebound method callable still dispatches its RESOLVED callee — the marks that say so
 * have to survive the clone, or the rebound closure re-resolves the name from the caller. */
crbm('same-class', fn() => $o->mk()->bindTo($o));
crbm('subclass', fn() => $o->mk()->bindTo($kid));
crbm('scope-repeated', fn() => $o->mk()->bindTo($o, CrbmBase::class));
crbm('scope-static-keyword', fn() => $o->mk()->bindTo($o, 'static'));
crbm('static-closure-unbound', fn() => CrbmBase::mkStatic()->bindTo(null));
crbm('bind-static-form', fn() => Closure::bind($o->mk(), $kid));
echo "call:", $o->mk()->call($o), "\n";

/* ...and php's four refusals, each a warning plus null. */
crbm('unrelated-object', fn() => $o->mk()->bindTo($other));
crbm('unbind', fn() => $o->mk()->bindTo(null));
crbm('rebind-scope', fn() => $o->mk()->bindTo($o, CrbmKid::class));
crbm('explicit-null-scope', fn() => $o->mk()->bindTo($o, null));
crbm('instance-to-static', fn() => CrbmBase::mkStatic()->bindTo($o));
/* The receiver is decided before the scope. */
crbm('unrelated-wins', fn() => $o->mk()->bindTo($other, CrbmKid::class));
crbm('unbind-wins', fn() => $o->mk()->bindTo(null, CrbmBase::class));
/* call() binds to the argument's own class, so anything but the declaring class is a
 * scope rebind. */
echo "call-subclass:", var_export($o->mk()->call($kid), true), "\n";

/* A PLAIN closure is unaffected by all of it. */
$plain = function () { return "plain"; };
crbm('plain-bind', fn() => $plain->bindTo($o, CrbmBase::class));
crbm('plain-unbind', fn() => $plain->bindTo(null));
?>
--EXPECTF--
same-class:priv@CrbmBase
subclass:priv@CrbmKid
scope-repeated:priv@CrbmBase
scope-static-keyword:priv@CrbmBase
static-closure-unbound:sPriv@CrbmBase
bind-static-form:priv@CrbmKid
call:priv@CrbmBase
PHP Warning:  Cannot bind method CrbmBase::priv() to object of class CrbmOther, this will be an error in PHP 9 in %s on line %d
unrelated-object:NULL
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
unbind:NULL
PHP Warning:  Cannot rebind scope of closure created from method, this will be an error in PHP 9 in %s on line %d
rebind-scope:NULL
PHP Warning:  Cannot rebind scope of closure created from method, this will be an error in PHP 9 in %s on line %d
explicit-null-scope:NULL
PHP Warning:  Cannot bind an instance to a static closure, this will be an error in PHP 9 in %s on line %d
instance-to-static:NULL
PHP Warning:  Cannot bind method CrbmBase::priv() to object of class CrbmOther, this will be an error in PHP 9 in %s on line %d
unrelated-wins:NULL
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
unbind-wins:NULL
call-subclass:PHP Warning:  Cannot rebind scope of closure created from method, this will be an error in PHP 9 in %s on line %d
NULL
plain-bind:plain
plain-unbind:plain
--CLEAN--
<?php
