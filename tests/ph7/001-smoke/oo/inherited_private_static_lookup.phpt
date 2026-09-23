--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a base's PRIVATE method is in the child's table — refused, not undefined
--DESCRIPTION--
php's inheritance puts a base's private methods in the child's function table so
the child's own inherited code can dispatch them and so a call from outside is
refused with the reason php gives: `B::p()` is
"Call to private method A::p() from global scope", never "undefined method".
PHL copied private INSTANCE methods down but not private STATICS, so every
spelling of the static call reported the name as undefined and `static::p()`
could not find its own method. Two rules php applies to the same relationship
came with it: a base's private method is never OVERRIDDEN, so a child may
declare an incompatible signature of that name, and `final private` is a warning
at the declaration rather than a fatal at the child.
--FILE--
<?php
class IpsBase {
    private static function ipsP() { return 'Base::ipsP'; }
    private function ipsI() { return 'Base::ipsI'; }
    public static function viaSelf() { return self::ipsP(); }
    public static function viaStatic() { return static::ipsP(); }
    public function viaThis() { return $this->ipsI(); }
}
class IpsKid extends IpsBase {}
function ipsSay($f) {
    try { $r = $f(); echo is_string($r) ? $r : var_export($r, true), "\n"; }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
/* The base's own code reaches it through the child, by either spelling. */
ipsSay(fn() => IpsKid::viaSelf());
ipsSay(fn() => IpsKid::viaStatic());
ipsSay(fn() => (new IpsKid)->viaThis());
/* From outside, every spelling gives php's refusal — and names the DECLARING
   class for the direct forms, the SPELLED one for the callback reason. */
ipsSay(fn() => IpsKid::ipsP());
ipsSay(fn() => (['IpsKid', 'ipsP'])());
ipsSay(fn() => call_user_func(['IpsKid', 'ipsP']));
ipsSay(fn() => (new IpsKid)->ipsI());
/* ...while the surfaces that describe a class keep it off the child. */
var_dump(method_exists('IpsKid', 'ipsP'), is_callable(['IpsKid', 'ipsP']),
         get_class_methods('IpsKid'));

/* A base's private method is not overridden: the child's is an independent
   member of the same name, whatever its signature. */
class IpsSig { private function m($a) {} private static function sm($a) {} }
class IpsSigKid extends IpsSig { private function m($a, $b, $c) {} private static function sm($a, $b, $c) {} }
echo "sig ok\n";
?>
--EXPECT--
Base::ipsP
Base::ipsP
Base::ipsI
Error: Call to private method IpsBase::ipsP() from global scope
Error: Call to private method IpsBase::ipsP() from global scope
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access private method IpsKid::ipsP()
Error: Call to private method IpsBase::ipsI() from global scope
bool(false)
bool(false)
array(3) {
  [0]=>
  string(7) "viaSelf"
  [1]=>
  string(9) "viaStatic"
  [2]=>
  string(7) "viaThis"
}
sig ok
--CLEAN--
<?php
