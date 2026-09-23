--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Closure over a non-public method keeps working after it escapes its class
--FILE--
<?php
class FccEscBase {
    private function priv($x = 0) { return "priv$x"; }
    protected function prot() { return "prot"; }
    private static function sPriv() { return "sPriv"; }
    public function mkPriv() { return $this->priv(...); }
    public function mkProt() { return self::prot(...); }
    public static function mkStatic() { return self::sPriv(...); }
    public function mkFromCallable() { return Closure::fromCallable([$this, 'priv']); }
}
class FccEscKid extends FccEscBase {}

function fccEscTry($label, $cb, ...$args) {
    try {
        echo $label, ":", $cb(...$args), "\n";
    } catch (Throwable $e) {
        echo $label, ":", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

$o = new FccEscKid();
/* Built inside the class, invoked at global scope: php resolves the callee once, where
 * the closure is built, so every one of these runs. */
fccEscTry('escaped-private', $o->mkPriv(), 9);
fccEscTry('escaped-protected', $o->mkProt());
fccEscTry('escaped-static', FccEscBase::mkStatic());
fccEscTry('escaped-fromCallable', $o->mkFromCallable());
fccEscTry('escaped-reflection', (new ReflectionMethod('FccEscBase', 'priv'))->getClosure($o));

/* ...through the callback surfaces too. */
echo "array_map:", implode("|", array_map($o->mkPriv(), [1, 2])), "\n";
echo "call_user_func:", call_user_func($o->mkPriv()), "\n";

/* Building one from OUTSIDE is still php's refusal, at the creation. */
try {
    $o->priv(...);
} catch (Throwable $e) {
    echo "outside-create:", get_class($e), ": ", $e->getMessage(), "\n";
}
/* ...and the plain call the escaped closure does not license. */
try {
    $o->priv();
} catch (Throwable $e) {
    echo "outside-call:", get_class($e), ": ", $e->getMessage(), "\n";
}
/* A class that routes inaccessible names through __call keeps that routing for the
 * direct spelling, while the screened closure reaches the real method. */
class FccEscMagic {
    private function priv() { return "real-priv"; }
    public function __call($n, $a) { return "magic-$n"; }
    public function mk() { return $this->priv(...); }
}
$m = new FccEscMagic();
fccEscTry('magic-closure', $m->mk());
fccEscTry('magic-direct', fn() => $m->priv());
?>
--EXPECT--
escaped-private:priv9
escaped-protected:prot
escaped-static:sPriv
escaped-fromCallable:priv0
escaped-reflection:priv0
array_map:priv1|priv2
call_user_func:priv0
outside-create:Error: Call to private method FccEscBase::priv() from global scope
outside-call:Error: Call to private method FccEscBase::priv() from global scope
magic-closure:real-priv
magic-direct:magic-priv
--CLEAN--
<?php
