--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A private/protected refusal names the calling scope, not "global scope"
--DESCRIPTION--
php's Error for an inaccessible method or constructor names the scope the call
was made from — "from scope C" — and says "from global scope" only for code
outside every class. Both PHL messages were hardcoded to the global wording, so
every refusal raised from inside a class reported a scope the caller was not in.
The rule is php's zend_get_executed_scope(): the executing method's declaring
class, a bound closure's explicit scope, a closure's creation-site class, or the
class whose initializer is running — the same computation the ACCESS decision
already made, now shared. A TRAIT method reports the class that USES the trait
(php flattens it in), never the trait and never the receiver's runtime class.
--FILE--
<?php
class VensTarget {
    private function __construct() {}
    private function vensPriv() {}
    private static function vensPrivStatic() {}
    protected function vensProt() {}
}
function vensSay(callable $fn) {
    try { $fn(); $out = 'no error'; }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $out, "\n";
}
$vensMake = fn() => (new ReflectionClass('VensTarget'))->newInstanceWithoutConstructor();

class VensScope {
    public function go($make) {
        $o = $make();
        vensSay(function () { new VensTarget; });
        vensSay(function () use ($o) { $o->vensPriv(); });
        vensSay(function () use ($o) { $o->vensProt(); });
        vensSay(function () { VensTarget::vensPrivStatic(); });
        vensSay(function () { $cb = ['VensTarget', 'vensPriv']; $cb(); });
    }
    public static function goStatic() {
        vensSay(function () { new VensTarget; });
    }
}
trait VensTrait {
    public function go() { vensSay(function () { new VensTarget; }); }
}
class VensBase { use VensTrait; }
class VensKid extends VensBase {}

(new VensScope)->go($vensMake);
VensScope::goStatic();
/* A trait method reports the composing class, from a subclass instance too. */
(new VensBase)->go();
(new VensKid)->go();
/* Outside every class the global wording stands. */
vensSay(function () { new VensTarget; });
$o = $vensMake();
vensSay(function () use ($o) { $o->vensPriv(); });
echo "end\n";
?>
--EXPECT--
Error: Call to private VensTarget::__construct() from scope VensScope
Error: Call to private method VensTarget::vensPriv() from scope VensScope
Error: Call to protected method VensTarget::vensProt() from scope VensScope
Error: Call to private method VensTarget::vensPrivStatic() from scope VensScope
Error: Call to private method VensTarget::vensPriv() from scope VensScope
Error: Call to private VensTarget::__construct() from scope VensScope
Error: Call to private VensTarget::__construct() from scope VensBase
Error: Call to private VensTarget::__construct() from scope VensBase
Error: Call to private VensTarget::__construct() from global scope
Error: Call to private method VensTarget::vensPriv() from global scope
end
