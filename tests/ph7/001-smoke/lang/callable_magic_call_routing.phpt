--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every callable spelling routes an unreachable method through __call/__callStatic
--DESCRIPTION--
php answers a name the class cannot reach directly — missing, or present but
inaccessible from here — through the catch-all, in EVERY callable context. Only
the `$o->m()` / `C::m()` SYNTAX did that (and the static half of it only for a
MISSING name, never an inaccessible one): `$cb = ['C','zz']; $cb()` threw
`Call to undefined method`, and `call_user_func(['C','zz'])` handed back NULL
silently, where php runs `__callStatic`. is_callable() already answered true for
all of these, so the predicate and the dispatch disagreed.
--FILE--
<?php
class CmcrMagic {
    private function priv() { return 'real-priv'; }
    private static function privStatic() { return 'real-priv-static'; }
    protected function prot() { return 'real-prot'; }
    public function __call($name, $args) { return "call:$name(" . implode(',', $args) . ')'; }
    public static function __callStatic($name, $args) { return "static:$name(" . implode(',', $args) . ')'; }
}
class CmcrPlain {
    public function real() { return 'real'; }
}

function cmcrRun(string $label, callable $fn): void
{
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}

/* A MISSING name, through every spelling. */
cmcrRun('syntax static', fn() => CmcrMagic::zz(1, 2));
cmcrRun('syntax instance', fn() => (new CmcrMagic)->zz(1, 2));
cmcrRun('dispatch array', function () { $cb = ['CmcrMagic', 'zz']; return $cb(1, 2); });
cmcrRun('dispatch string', function () { $cb = 'CmcrMagic::zz'; return $cb(1, 2); });
cmcrRun('dispatch object', function () { $cb = [new CmcrMagic, 'zz']; return $cb(1, 2); });
cmcrRun('cuf array', fn() => call_user_func(['CmcrMagic', 'zz'], 1, 2));
cmcrRun('cuf string', fn() => call_user_func('CmcrMagic::zz', 1, 2));
cmcrRun('cuf object', fn() => call_user_func([new CmcrMagic, 'zz'], 1, 2));
cmcrRun('cufa', fn() => call_user_func_array(['CmcrMagic', 'zz'], [3, 4]));
cmcrRun('array_map', fn() => implode('|', array_map([new CmcrMagic, 'zz'], [7, 8])));

/* An INACCESSIBLE name goes the same way. */
cmcrRun('private syntax', fn() => (new CmcrMagic)->priv(9));
cmcrRun('private static syntax', fn() => CmcrMagic::privStatic(9));
cmcrRun('private dispatch', function () { $cb = [new CmcrMagic, 'priv']; return $cb(9); });
cmcrRun('private static dispatch', function () { $cb = ['CmcrMagic', 'privStatic']; return $cb(9); });
cmcrRun('protected cuf', fn() => call_user_func([new CmcrMagic, 'prot'], 9));

/* Without a catch-all, php's Errors stand. */
cmcrRun('no magic dispatch', function () { $cb = ['CmcrPlain', 'zz']; return $cb(); });
cmcrRun('no magic string', function () { $cb = 'CmcrPlain::zz'; return $cb(); });

/* The predicate agrees with all of the above. */
echo 'is_callable missing => ', var_export(is_callable(['CmcrMagic', 'zz']), true), "\n";
echo 'is_callable private => ', var_export(is_callable([new CmcrMagic, 'priv']), true), "\n";
echo 'is_callable plain missing => ', var_export(is_callable(['CmcrPlain', 'zz']), true), "\n";
echo "end\n";
?>
--EXPECT--
syntax static => 'static:zz(1,2)'
syntax instance => 'call:zz(1,2)'
dispatch array => 'static:zz(1,2)'
dispatch string => 'static:zz(1,2)'
dispatch object => 'call:zz(1,2)'
cuf array => 'static:zz(1,2)'
cuf string => 'static:zz(1,2)'
cuf object => 'call:zz(1,2)'
cufa => 'static:zz(3,4)'
array_map => 'call:zz(7)|call:zz(8)'
private syntax => 'call:priv(9)'
private static syntax => 'static:privStatic(9)'
private dispatch => 'call:priv(9)'
private static dispatch => 'static:privStatic(9)'
protected cuf => 'call:prot(9)'
no magic dispatch => Error: Call to undefined method CmcrPlain::zz()
no magic string => Error: Call to undefined method CmcrPlain::zz()
is_callable missing => true
is_callable private => true
is_callable plain missing => false
end
