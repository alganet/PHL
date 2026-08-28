--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: trait-copied private/protected members accessible from the using class
--FILE--
<?php
trait TpsaT {
    private static function tpsaSecret() { return 'priv'; }
    protected static function tpsaGuarded() { return 'prot'; }
    private function tpsaInst() { return 'inst'; }
}
class TpsaA {
    use TpsaT;
    public static function callStatics() { return self::tpsaSecret() . '/' . self::tpsaGuarded(); }
    public function callInst() { return $this->tpsaInst(); }
}
echo TpsaA::callStatics(), "\n";
echo (new TpsaA)->callInst(), "\n";
// NOTE: the deny path (calling TpsaA::tpsaSecret() from global scope) matches
// php's message byte-for-byte but is uncatchable in PHL (the pre-frame OP_CALL
// error family, NEWPLAN section 6) - asserted by probe, not here.
?>
--EXPECT--
priv/prot
inst
--CLEAN--
<?php
