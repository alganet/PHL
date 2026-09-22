--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-static method named through a CLASS NAME cannot be dispatched directly
--DESCRIPTION--
`$cb = ['C','instanceMethod']; $cb();` RAN the method with no $this — a
$this-less body that touches $this then read null instead of failing loudly —
where php throws `Non-static method C::m() cannot be called statically`. php
refuses this even when the CALLER has a compatible $this, which is what makes
the direct dispatch stricter than a callback: call_user_func() with the very
same spelling runs the method and binds the caller's $this. Both rules are
pinned here. Visibility outranks staticness (a private non-static method
reports the private denial), and an ABSTRACT method — which the dispatch used
to reach as a mangled internal function name — reports php's "Cannot call
abstract method".
--FILE--
<?php
class CnscBase {
    public function m() { return 'CnscBase::m ' . (isset($this) ? 'with-this' : 'no-this'); }
    public static function s() { return 'CnscBase::s'; }
    private function priv() { return 'priv'; }
    protected function prot() { return 'prot'; }
}
class CnscChild extends CnscBase {
    public function viaDispatch() { $cb = ['CnscBase', 'm']; return $cb(); }
    public function viaDispatchString() { $cb = 'CnscBase::m'; return $cb(); }
    public function viaCallback() { return call_user_func(['CnscBase', 'm']); }
    public function viaCallbackString() { return call_user_func('CnscBase::m'); }
}
abstract class CnscAbstract {
    abstract public function am();
    public function cm() { return 'cm'; }
}
interface CnscIface { public function im(); }

function cnscRun(string $label, callable $fn): void
{
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}

/* Direct dispatch: refused, whatever $this the caller has. */
cnscRun('array global', function () { $cb = ['CnscBase', 'm']; return $cb(); });
cnscRun('string global', function () { $cb = 'CnscBase::m'; return $cb(); });
cnscRun('array in method', fn() => (new CnscChild)->viaDispatch());
cnscRun('string in method', fn() => (new CnscChild)->viaDispatchString());
cnscRun('inherited names declarer', function () { $cb = ['CnscChild', 'm']; return $cb(); });

/* A callback is laxer: the caller's compatible $this makes it callable, and php binds it. */
cnscRun('callback in method', fn() => (new CnscChild)->viaCallback());
cnscRun('callback string in method', fn() => (new CnscChild)->viaCallbackString());

/* Visibility is decided before staticness. */
cnscRun('private', function () { $cb = ['CnscBase', 'priv']; return $cb(); });
cnscRun('protected', function () { $cb = ['CnscBase', 'prot']; return $cb(); });

/* No body to call. */
cnscRun('abstract array', function () { $cb = ['CnscAbstract', 'am']; return $cb(); });
cnscRun('abstract string', function () { $cb = 'CnscAbstract::am'; return $cb(); });
cnscRun('interface method', function () { $cb = ['CnscIface', 'im']; return $cb(); });
cnscRun('concrete on abstract', function () { $cb = ['CnscAbstract', 'cm']; return $cb(); });

/* Still fine: a static method, and an OBJECT target (which carries its own $this). */
cnscRun('static ok', function () { $cb = ['CnscBase', 's']; return $cb(); });
cnscRun('static string ok', function () { $cb = 'CnscBase::s'; return $cb(); });
cnscRun('object target', function () { $cb = [new CnscBase, 'm']; return $cb(); });
echo "end\n";
?>
--EXPECT--
array global => Error: Non-static method CnscBase::m() cannot be called statically
string global => Error: Non-static method CnscBase::m() cannot be called statically
array in method => Error: Non-static method CnscBase::m() cannot be called statically
string in method => Error: Non-static method CnscBase::m() cannot be called statically
inherited names declarer => Error: Non-static method CnscBase::m() cannot be called statically
callback in method => 'CnscBase::m with-this'
callback string in method => 'CnscBase::m with-this'
private => Error: Call to private method CnscBase::priv() from global scope
protected => Error: Call to protected method CnscBase::prot() from global scope
abstract array => Error: Cannot call abstract method CnscAbstract::am()
abstract string => Error: Cannot call abstract method CnscAbstract::am()
interface method => Error: Cannot call abstract method CnscIface::im()
concrete on abstract => Error: Non-static method CnscAbstract::cm() cannot be called statically
static ok => 'CnscBase::s'
static string ok => 'CnscBase::s'
object target => 'CnscBase::m with-this'
end
