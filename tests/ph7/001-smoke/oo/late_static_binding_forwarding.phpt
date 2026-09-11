--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
forwarding static calls (self::/parent::/static::) preserve the late-static-binding class
--FILE--
<?php
/* php: a FORWARDING static call — self::m(), parent::m(), static::m() — keeps the
 * caller's late-static-binding class, so `static::` inside the callee still refers
 * to the runtime class. A non-forwarding C::m() resets it to C. PHL pushed the
 * callee's DECLARING class instead, so `static::class` reached through self:: /
 * parent:: reported the wrong class — which broke PHPUnit's
 * `self::generateReturnValuesForTestDoubles()` (static::class -> the wrong test
 * class -> attribute metadata missed). */
class NlsA {
    public function who(): string { return static::class; }             // direct
    public function viaSelf(): string { return self::sm(); }             // forwarding
    public static function sm(): string { return static::class; }
    public static function nonForward(): string { return NlsA::sm(); }   // non-forwarding
}
class NlsB extends NlsA {
    public function viaParent(): string { return parent::sm(); }         // forwarding
    public static function make(): static { return new static(); }
}
$b = new NlsB();
echo "direct: ", $b->who(), "\n";                 // NlsB
echo "viaSelf: ", $b->viaSelf(), "\n";            // NlsB (LSB preserved through self::)
echo "viaParent: ", $b->viaParent(), "\n";        // NlsB (LSB preserved through parent::)
echo "newStatic: ", get_class(NlsB::make()), "\n"; // NlsB
echo "nonForward: ", NlsB::nonForward(), "\n";    // NlsA (reset by NlsA::)
echo "staticCall: ", NlsB::sm(), "\n";            // NlsB
?>
--EXPECT--
direct: NlsB
viaSelf: NlsB
viaParent: NlsB
newStatic: NlsB
nonForward: NlsA
staticCall: NlsB
--CLEAN--
<?php
